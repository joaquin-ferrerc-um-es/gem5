package repscr.gem5

import repscr._
import properties._
import util.misc._
import points._

import scala.collection.parallel.CollectionConverters._
import scala.util.parsing.combinator.RegexParsers
import java.io.File
import util.RMap

class Simulation(filename: String, val properties: PropertyMap) extends Gem5DataPoint {
  def files = Seq(filename)
}
object Simulation {
  class ProgressReport {
    val tstart = java.lang.System.currentTimeMillis
    var sims_count = 0
    var files_count = 0
    def progress(files: Int, sims: Int): Unit = synchronized {
      sims_count = sims_count + sims
      files_count = files_count + files
      val t = elapsed
      print(f"$files_count files and ${sims_count} simulations parsed in $t%.3fs (${t / files_count}%.3fs per file, ${t / sims_count}%.3fs per simulation)...\r")
    }
    def finish(): Unit = {
      val t = elapsed
      print(f"$files_count files and ${sims_count} simulations parsed in $t%.3fs (${t / files_count}%.3fs per file, ${t / sims_count}%.3fs per simulation)   \n")
    }
    def elapsed = (java.lang.System.currentTimeMillis - tstart).toDouble / 1000
  }

  def parseStatsDirWithProgress(dirs: String*): Seq[Simulation] = {
    val progress = new ProgressReport
    println(f"Finding files in ${dirs.mkString("«", "», «", "»")}.")
    val files = new RecursiveFileIterator(dirs map (new File(_)): _*).filter(f => f.getName.endsWith(".stats") || f.getName == "stats.txt").toSeq
    println(f"Found ${files.size} files.")
    val sims = (files.par flatMap { f =>
      try fromFile(f, Some(progress)) map (Right(_))
      catch {
        case m: Exception => Seq(Left(s"Loading $f: $m"))
      }
    }).seq
    sims collect { case Left(error) => error } foreach println
    progress.finish()
    sims collect { case Right(s) => s }
  }

  def fromFile(file: File, progressReport: Option[ProgressReport] = None): Seq[Simulation] = {
    if (file.length == 0) sys.error("Empty stats file")
    //val f = new java.io.FileReader(file)
    val f = new FileCharSequence(file) // workaround for bug in scala library for big files
    def parseRawGEM5Simulation(rawprops: parser.RawGEM5Simulation): Simulation = {
      val s = new Simulation(file.getPath, PropertyMap((Gem5Properties.knownProperties.map { case (name, pinfo) => name -> (
        try pinfo.getter(rawprops)
        catch {
          case e: NoSuchElementException =>
            if (!pinfo.optional) Console.err.println(s"Loading $file: Missing $name, ${e.getMessage}")
            Symbol("MissingProperty")
          case e: Exception =>
            Console.err.println(s"Loading $file, $name, $e [${e.getStackTrace.take(6).mkString(", ")}]")
            Symbol("PropertyError")
        })
      })))
      progressReport foreach (_.progress(0, 1))
      s
    }
    val onlyOneSimPerFile = true
    val sims =
      try
        if (onlyOneSimPerFile)
          parser.parse(parser.GEM5StatsFile(parseRawGEM5Simulation), f) match {
            case parser.Success(sim, _) => Seq(sim)
            case x => Console.err.println(s"Loading $file:\n$x"); Seq()
          }
        else
          parser.parseAll(parser.GEM5StatsFileSeq(parseRawGEM5Simulation), f) match {
            case parser.Success(simseq, _) => simseq
            case x => Console.err.println(s"Loading $file:\n$x"); Seq()
          }
      catch {
        case e: Exception => sys.error(s"Loading $file: $e [${e.getStackTrace.take(6).mkString(", ")}]")
      } finally f.close()
    progressReport foreach (_.progress(1, 0))
    sims
  }


  object parser extends RegexParsers {
    case class RawGEM5Simulation(configuration: RMap, stats: RMap)

    type RawParser = RawGEM5Simulation => Simulation

    override val skipWhitespace = false
    val blines = rep("\n")

    def GEM5StatsFileSeq(parseRawGEM5Simulation: RawParser): Parser[Seq[Simulation]] = rep1(GEM5StatsFile(parseRawGEM5Simulation))

    def GEM5StatsFile(parseRawGEM5Simulation: RawParser): Parser[Simulation] = blines ~> configs ~ (blines ~> stats <~ blines) ^^ { case confsRMap ~ statsRMap =>
      parseRawGEM5Simulation(RawGEM5Simulation(confsRMap, statsRMap))
    }

    val (configs_begin_text, configs_end_text) = ("---------- Begin Configuration   ----------", "---------- End Configuration   ----------")
    val (stats_begin_text, stats_end_text) = ("---------- Begin Simulation Statistics ----------", "---------- End Simulation Statistics   ----------")

    val prop_name = rep1sep("[^ =\n\\[\\].]+".r, ".")
    val prop_value = ".*".r ^^ { v => // remove comment if present and trim whitespace
      val p = v lastIndexOf "#"
      (if (p != -1) v.substring(0, p) else v).trim
    }

    lazy val configs = configs_begin_text ~> blines ~> chainl1(
      config_section ^^ { case (sn, confs) =>
        val m = new RMap()
        confs.foreach { case (k, v) => m.set(sn +: k: _*)(v) }
        m
      },
      config_section,
      blines ^^^ { (m: RMap, y: (String, List[(List[String], String)])) =>
        y._2.foreach { case (k, v) => m.set(y._1 +: k: _*)(v) }
        m
      }
    ) <~ blines <~ configs_end_text
    lazy val config_section = not(configs_end_text) ~> (config_section_header ~ ("\n" ~> repsep(config_line, "\n") <~ "\n") ^^ { case sn ~ plist => sn -> plist })
    val config_section_header = "[" ~> "[^\\]]*".r <~ "]"
    val config_line = prop_name ~ ("=" ~> prop_value) ^^ { case pn ~ pv => pn -> pv }

    lazy val stats = stats_begin_text ~> "\n" ~> chainl1(
      stats_line ^^ { case (k, v) => new RMap().set(k: _*)(v) },
      stats_line,
      blines ^^^ { (x: RMap, y: (Seq[String], String)) => x.set(y._1: _*)(y._2) }
    ) <~ blines <~ stats_end_text
    val stats_line = not(stats_end_text) ~> (prop_name ~ prop_value ^^ { case pn ~ pv => pn -> pv })
  }
}

class SimulationMix(val simulations: Iterable[Gem5DataPoint]) extends Gem5DataPoint {
  val properties: PropertyMap = {
    val keys = (simulations.view.map(_.properties.keySet)).reduce(_ ++ _)
    PropertyMap(keys.view.map { k =>
      k -> {
        val values = simulations.map (_.properties get k)
        if (values.count(_.contains(Symbol("MissingProperty"))) == values.size) Symbol("MissingProperty")
        else try Gem5Properties.knownProperties(k).mixer(values)
        catch {
          case e: Throwable =>
            println(f"Error mixing $k:\n${simulations.map(s => s.properties.get(k).toString + "\t" + s.files.mkString(" ")).mkString("\n")}")
            Symbol("MixingError")
        }
      }
    })
  }
  override def files = simulations flatMap (_.files)

  def removeOutliers(valueFn: Gem5DataPoint => Double, threshold: Double = 2, log: Boolean = false): SimulationMix = {
    val values = simulations.map(valueFn)
    def square(a: Double) = a * a
    val mean = values.sum / values.size
    val stddev =
      if (values.size > 1) Math.sqrt(values.map { i => square(mean - i) }.sum / (values.size - 1))
      else 0.0
    val maxOutliers = values.size / 2
    def outlieness(s: Gem5DataPoint) = (valueFn(s) - mean).abs / stddev
    val sims = simulations.toSeq.sortBy(outlieness)
    if (log) println(f"A ${sims.map(valueFn(_).formatted("%10.4g")) mkString " "} | $mean%10.4g $stddev%10.4g")
    if (log) println(f"R ${sims.map(outlieness(_).formatted("%10.4g")) mkString " "}")
    def outlier(s: Gem5DataPoint) = outlieness(s) > threshold
    val (outliers, inliers) = sims.partition(outlier)
    outliers foreach { s => println(f"Outlier: ${outlieness(s)}%10.4g ${s.files mkString ","}") }
    val selected = inliers ++ outliers.take(outliers.size - maxOutliers)
    if (selected.size < sims.size) {
      if (log) println(s"Removed ${sims.size - selected.size} of ${sims.size}")
      new SimulationMix(selected)
    } else this
  }
}

object SimulationMix {
  var defaultIgnorable = Set("random_seed", "hostname") // 'git_revision

  def mixSameConfig(ignorablep: String => Boolean = defaultIgnorable)(l: Iterable[Gem5DataPoint]): Iterable[SimulationMix] = {
    def configList(s: Gem5DataPoint) = s.configs.filter { case (k, v) => !ignorablep(k) }
    l.par.groupBy(configList).values.map(l => new SimulationMix(l.seq)).seq
  }
  def mixSameConfig(l: Iterable[Gem5DataPoint]): Iterable[SimulationMix] = mixSameConfig()(l)

  type Mixer = Iterable[Option[Any]] => Any
  object mixers {
    def default(vs: Iterable[Option[Any]]): Any = {
      val l = vs.flatten
      if (l.nonEmpty && l.allEquals) l.head else l
    }
    def randomSeed(vs: Iterable[Option[Any]]) = vs.flatten.toSeq.sortBy(_.value)
    def samplesWithDefault(default: Any)(vs: Iterable[Option[Any]]) = Vwe.fromSamples(vs map (o => CoordValue(o.getOrElse(default)).toDouble)) // missing values as «default»
    def samples(vs: Iterable[Option[Any]]) = samplesWithDefault(0)(vs)
    def samplesNaNsAsDefault(default: Any)(vs: Iterable[Option[Any]]) = Vwe.fromSamples(vs map (o => CoordValue(o.getOrElse(default)).toDouble) map (x => if (x.isNaN) default.toDouble else x)) // missing values and NaNs treated as default
    def samplesIgnoringMissing(vs: Iterable[Option[Any]]) = Vwe.fromSamples(vs.flatten map (o => CoordValue(o).toDouble)) // missing values ignored

    def mapMixer(valueMixer: Mixer)(vs: Iterable[Option[Any]]) = {
      def mapMixerTyped[A, B, C](valueMixer: Iterable[Option[A]] => B)(l: Iterable[Map[C, A]]) = {
        val keys = l.map(_.keySet).reduce(_ ++ _)
        keys.map { k => (k, valueMixer(l map (_.get(k)))) }.toMap
      }
      mapMixerTyped(valueMixer)(vs map {
        case Some(m) => Map.empty[Any, Option[Any]] ++ m.asInstanceOf[Iterable[(Any, Any)]].map { x => x._1 -> Some(x._2) }
        case None => Map.empty[Any, Option[Any]]
      })
    }
  }
}

