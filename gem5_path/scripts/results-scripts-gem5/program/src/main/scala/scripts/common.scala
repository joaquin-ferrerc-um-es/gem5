import language.implicitConversions
import language.postfixOps

import repscr.gem5.Gem5Coords._

package object scripts {
  object plotUtil {
    import repscr.plots._
    import util.misc._
    import util.time.now


    def plotWithProgress(plots: Iterable[Plot]): Unit = {
      val tstart = now
      var count = 0
      plots.foreach { p =>
        count = count + 1
        p.outputFiles.foreach { case (_, f) => createDirs(f.getParentFile) }
        println(s"Generating ${p.outputFiles.getOrElse(Format.pdf, p.outputFiles.headOption.map(_._2).getOrElse("[no ouput file]"))} ")
        p.draw()
        val t = (now - tstart).toDouble / 1000
        print(f"$count/${plots.size} plots in $t%.3fs (${t / count}%.3fs per plot)...\r")
      }
      val t = (now - tstart).toDouble / 1000
      print(f"$count plots in $t%.3fs (${t / count}%.3fs per plot).         \n")
    }
  }

  trait PlotScript { self: App =>
    import repscr._
    import gem5._
    import util.misc._
    import java.io.File

    private var _indirs = Seq.empty[String]
    def indirs = _indirs
    def outdir = lazyInit.outdir
    def parsedFiles = lazyInit.parsedSimulations
    def interesting = lazyInit.interesting
    def mixes = lazyInit.mixes

    def simulationsTransform(s: Simulation): Simulation = s
    def simulationsFilter(s: Simulation): Boolean = true

    private case class LazyInit(
      parsedSimulations: Seq[Simulation],
      interesting: Seq[Simulation],
      mixes: Seq[SimulationMix],
      outdir: String
    )
    private lazy val lazyInit = {
      var outdir = "ous-plots"
      var baseDir = "."
      def normalizePath(p: String) = (if (new File(p).isAbsolute) new File(p) else new File(baseDir, p)).getCanonicalPath()
      def parseArgs(l: List[String]): Unit = l match {
        case "--base-dir" :: bd :: rest =>
          baseDir = normalizePath(bd); parseArgs(rest)
        case "--ins" :: ins :: rest =>
          _indirs = _indirs :+ normalizePath(ins); parseArgs(rest)
        case "--outs" :: outs :: rest =>
          outdir = normalizePath(outs); parseArgs(rest)
        case opc :: rest if opc matches "--[^=]+=.+" =>
          parseArgs(opc.substring(0, opc.indexOf('=')) :: opc.substring(opc.indexOf('=') + 1) :: rest)
        case Nil =>
        case _ =>
          Console.err.println(s"Argumentos incorrectos: ${l.mkString("'", "', '", "'")}")
          Console.err.println(s"Después de --script=… se aceptan --ins=…, --outs=… y --base-dir=…")
          sys.exit(1)
      }
      parseArgs(args.toList)
      if (indirs.isEmpty) _indirs = Seq(normalizePath("ins"))
      outdir = normalizePath(outdir)

      val parsedFiles = Simulation.parseStatsDirWithProgress(indirs: _*)

      val interesting = parsedFiles map simulationsTransform filter simulationsFilter
      println(s"Interesting simulations: ${interesting.size}")

      val mixes = SimulationMix.mixSameConfig(interesting).toSeq
      println(s"Mixed simulations: ${mixes.size}")
      addSimulationsDependentCoords(mixes)

      if (mixes.nonEmpty) {
        println("Configuration variations:")
        for ((k, v) <- Gem5DataPoint.findConfigVariations(mixes)) println(s"  ${k}: ${v.countItemsSorted.mkString(",")}")
      }
      LazyInit(parsedFiles, interesting, mixes, outdir)
    }
  }
}

package scripts {
  import scala.annotation.tailrec

  object Main extends App {
    val scripts: Map[String, PlotScript with App] = Seq(NoPlots, Plots202203HuaweiFinal)
      .map {
        x => x.getClass.getName.replaceFirst("^scripts.", "").replaceFirst("\\$$", "") -> x
      } toMap

    val otherArgs = collection.mutable.Buffer.empty[String]
    var script = "NoPlots"
    @tailrec
    def parseArgs(l: List[String]): Unit = l match {
      case "--script" :: s :: rest =>
        script = s; parseArgs(rest)
      case opc :: rest if opc matches "--[^=]+=.+" =>
        parseArgs(opc.substring(0, opc.indexOf('=')) :: opc.substring(opc.indexOf('=') + 1) :: rest)
      case other :: rest =>
        otherArgs += other; parseArgs(rest)
      case Nil =>
    }
    parseArgs(args.toList)

    scripts.get(script) match {
      case Some(mainObject) => mainObject.main(otherArgs.toArray)
      case None =>
        Console.err.println(s"Script desconocido: $script")
        Console.err.println(s"Scripts disponibles: ${scripts.keys.toSeq.sorted.mkString(" ")}")
        sys.exit(1)
    }
  }
}
