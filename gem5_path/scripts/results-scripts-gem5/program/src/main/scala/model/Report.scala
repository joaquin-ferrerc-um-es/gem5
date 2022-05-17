package model

import collection.immutable.SortedMap
import collection.immutable.TreeMap
import collection.immutable.TreeSet
import repscr.gem5._
import repscr.gem5.Gem5Coords._
import repscr.{PlotUtil, Vwe}
import util.misc._
import util.time
import Listing.IndexColumn
import Listing.ResultColumn
import Listing.IndexColumn._


class Report(val listingsDir: String,
  val inDirs: Seq[String],
  val removeOutliers: Boolean = false) {
  val simulations = Simulation.parseStatsDirWithProgress(inDirs: _*)
  val all_mixes = time("mixing") { SimulationMix.mixSameConfig(simulations) }

  val (outliers_log, removed_outliers_count, mixes) =
    if (removeOutliers) time("removing outliers") {
      val log = new StringBuilderStream
      val (rc, mixes) = Console.withOut(log) {
        // TODO: Update once we have stats to use for these purpose
        import repscr.points._
        /*Seq[(String, Gem5DataPoint => Double)](
          "html_kernel_cycles" -> ("htm_kernel_cycles".fn(_).value),
          "barrier_cycles" -> (_.xactProfiler_cycles.getOrElse("BARRIER", 0).value)).foldLeft((0, all_mixes)) {
          case ((current_removed_count, currentMixes), (name, valueFn)) =>
            println(s"Removing outliers due to $name:")
            val inliers = currentMixes.map(_.removeOutliers(valueFn, 2, true))
            val nremoved = currentMixes.map(_.simulations.size).sum - inliers.map(_.simulations.size).sum
            println(s"Removed $nremoved outliers due to $name.")
            (current_removed_count + nremoved, inliers)
        }*/
        (0, all_mixes)
      }
      println(s"Removed $rc outlier simulations.")
      (log.toString, rc, mixes)
    }
    else ("No outlier analysis performed.", 0, all_mixes)

  addSimulationsDependentCoords(mixes)

  val configVariations = time("finding config variations") { Report.configVariations(mixes) }
  val configConstants = time("finding config constants") { Report.configConstants(mixes) }

  def configValues(c: Coord) = configVariations.get(c) match {
    case Some(v) => v
    case None    => TreeMap.empty(c.ordering) + (configConstants(c) -> 1)
  }

  // recovers a value from its toString representation, if exists for the given  Coord
  def configValueFromString(c: Coord, s: String): Any = configValues(c).keysIterator.find(_.toString == s).getOrElse(s)

  type ListingId = String
  var listings: TreeMap[ListingId, Listing] = persistence.loadListings
  def reloadListings(): Unit = {listings = persistence.loadListings}

  var dirtyListings = Set.empty[ListingId]
  def isDirty(lid: ListingId) = dirtyListings(lid)

  object actions {
    def editListingRemoveIndex(lid: ListingId, idxname: String): Unit = {
      val l = listings(lid)
      listings = listings + (lid -> l.copy(indexes = l.indexes.filter(_.coord.name != idxname)))
      dirtyListings += lid
    }

    def editListingAddIndex(lid: ListingId, idxname: String): Unit = {
      val l = listings(lid)
      listings = listings + (lid -> l.copy(indexes = l.indexes :+ IndexColumn(idxname.toCoord)))
      dirtyListings += lid
    }

    def editListingIndexSetUse(lid: ListingId, idxname: String, use: String): Unit = {
      val l = listings(lid)
      val idxi = l.indexes.indexWhere(_.coord.name == idxname)
      val u = use match {
        case "Section"     => Section
        case "XColumn"     => XColumn
        case "SerieColumn" => SerieColumn
        case "FilterOnly"  => FilterOnly
      }
      listings = listings + (lid -> l.copy(indexes = l.indexes.updated(idxi, l.indexes(idxi).copy(use = u))))
      dirtyListings += lid
    }

    def editListingIndexSetFilteringAllValues(lid: ListingId, idxname: String): Unit = {
      val l = listings(lid)
      val idxi = l.indexes.indexWhere(_.coord.name == idxname)
      listings = listings + (lid -> l.copy(indexes = l.indexes.updated(idxi, l.indexes(idxi).copy(filtering = AllValues))))
      dirtyListings += lid
    }

    def editListingIndexSetFilteringSomeValues(lid: ListingId, idxname: String, values: String*): Unit = {
      val l = listings(lid)
      val idxi = l.indexes.indexWhere(_.coord.name == idxname)
      def vals = values map {configValueFromString(l.indexes(idxi).coord, _)}
      listings = listings + (lid -> l.copy(indexes = l.indexes.updated(idxi, l.indexes(idxi).copy(filtering = SomeValues(vals)))))
      dirtyListings += lid
    }

    def editListingIndexSetPosition(lid: ListingId, idxname: String, pos: Int): Unit = {
      val l = listings(lid)
      val (idx, rest) = l.indexes.indexWhere(_.coord.name == idxname) match {
        case -1 => (IndexColumn(idxname.toCoord), l.indexes)
        case i  => (l.indexes(i), l.indexes.take(i) ++ l.indexes.drop(i + 1))
      }
      val p = 0 max pos min rest.size
      val indexes = (rest.take(p) :+ idx) ++ rest.drop(p)
      listings = listings + (lid -> l.copy(indexes = indexes))
      dirtyListings += lid
    }

    def editListingAddResult(lid: ListingId, resname: String): Unit = {
      val l = listings(lid)
      listings = listings + (lid -> l.copy(results = l.results :+ ResultColumn(resname.toCoord, normalize = false)))
      dirtyListings += lid
    }

    def editListingRemoveResult(lid: ListingId, resname: String): Unit = {
      val l = listings(lid)
      listings = listings + (lid -> l.copy(results = l.results.filter(_.coord.name != resname)))
      dirtyListings += lid
    }

    def editListingResultSetPosition(lid: ListingId, resname: String, pos: Int): Unit = {
      val l = listings(lid)
      val (idx, rest) = l.results.indexWhere(_.coord.name == resname) match {
        case -1 => (ResultColumn(resname.toCoord), l.results)
        case i  => (l.results(i), l.results.take(i) ++ l.results.drop(i + 1))
      }
      val p = 0 max pos min rest.size
      val results = (rest.take(p) :+ idx) ++ rest.drop(p)
      listings = listings + (lid -> l.copy(results = results))
      dirtyListings += lid
    }

    def editListingResultSetOptions(lid: ListingId, colname: String, normalized: Boolean, style: String): Unit = {
      val l = listings(lid)
      val coli = l.results.indexWhere(_.coord.name == colname)
      listings = listings + (lid -> l.copy(results = l.results.updated(coli, l.results(coli).copy(
        normalize = normalized,
        plotStyle = style match {case "Lines" => ResultColumn.LinesStyle case "Bars" | _ => ResultColumn.BarsStyle}))))
      dirtyListings += lid
    }

    def saveListing(lid: ListingId, newname: String): Unit = {
      val s = new java.io.FileWriter(new java.io.File(listingsDir + "/" + newname))
      try {
        s.write(persistence.write(listings(lid)))
        if (lid != newname) {
          listings = listings + (newname -> listings(lid))
        }
        dirtyListings -= newname
      } finally s.close()
    }

    def editListingChangeSource(lid: ListingId, srcname: String): Unit = {
      val l = listings(lid)
      val src = Listing.sources.available.filter(_.toString == srcname) match {
        case Seq(s) => s
        case _      => ???
      }
      listings = listings + (lid -> l.copy(source = src))
      dirtyListings += lid
    }
  }

  object persistence {
    object write {
      def apply(l: Listing) = wListing(l)
      def wListing(l: Listing) = s"Listing(${wListingSource(l.source)}, ${wIndexColumns(l.indexes)}, ${wResultColumns(l.results)})\n"
      def wSeq[A](l: Iterable[A], f: A => String) = l.map(f).mkString("[\n\t", ",\n\t", "\n]")
      def wOpt[A](l: Option[A], f: A => String) = l match {
        case Some(o) => s"Some(${f(o)})"
        case None    => "None"
      }
      def wListingSource(src: Listing.Source) = s"""Source("$src")"""
      def wIndexColumns(l: Seq[IndexColumn]) = wSeq(l, wIndexColumn)
      def wResultColumns(l: Seq[ResultColumn]) = wSeq(l, wResultColumn)
      def wIndexColumn(c: IndexColumn) = s"IndexColumn(${wCoord(c.coord)},${wFiltering(c.filtering)},${c.use})"
      def wFiltering(l: Filtering) = l match {
        case AllValues     => "AllValues"
        case SomeValues(v) => s"SomeValues(${wSeq(v, wValue)})"
      }
      def quoteChar(c: Char) = c match {
        case '\u0000'      => "\\u0000"
        case '\n'          => "\\n"
        case '\t'          => "\\t"
        case '"'           => "\\\""
        case c if c < 32   => f"\\u${c}%04x"
        case _             => c.toString
      }
      def wValue(v: Any) = " \"" + (for (c <- v.toString.toSeq) yield quoteChar(c)).mkString + "\""
      def wResultColumn(c: ResultColumn) = s"ResultColumn(${wCoord(c.coord)},${if (c.normalize) "normalized" else "absolute"},${c.plotStyle match { case ResultColumn.BarsStyle => "bars" case ResultColumn.LinesStyle => "lines" }})"
      def wCoord(c: Coord) = "\"" + c.name + "\""
    }

    import scala.util.parsing.combinator._
    object parser extends RegexParsers {
      import scala.util.matching.Regex

      override val whiteSpace = """[ \t\x0B\f\r\n]+""".r

      protected def regexNoWs(r: Regex) = new Parser[String] {
        def apply(in: Input) = {
          val source = in.source
          val start = in.offset
          r findPrefixMatchOf source.subSequence(start, source.length) match {
            case Some(matched) =>
              Success(source.subSequence(start, start + matched.end).toString, in.drop(matched.end))
            case None =>
              val found = if (start == source.length) "end of source" else "`" + source.charAt(start) + "'"
              Failure("string matching regex `" + r + "' expected but " + found + " found", in)
          }
        }
      }

      def quoted_character(closing: Char) = {
        regexNoWs("""\\[0-7]+""".r) ^^ { s => Integer.parseInt(s.substring(1), 8).toChar }
      } | {
        regexNoWs("""\\.""".r) ^^ { s =>
          s.charAt(1) match {
            case 'n' => '\n'
            case '"' => '\"'
            case _   => sys.error("carácter " + s)
          }
        }
      } | {
        regexNoWs(s"[^$closing]".r) ^^ { _.charAt(0) }
      }

      val string = "\"" ~> rep(quoted_character('\"')) <~ "\"" ^^ { s => s.mkString }

      val boolean = "true" ^^^ true | "false" ^^^ false

      val plotStyle = ("lines" ^^ { _ => ResultColumn.LinesStyle }) | ("bars" ^^ { _ => ResultColumn.BarsStyle })

      val plotNormalization = (("normalized" | "true") ^^^ true) | ("absolute" | "false") ^^^ false // obsolete
      val plot = ("Plot" ~ "(" ~ string ~ "," ~ string ~ "," ~ plotNormalization ~ opt("," ~ plotStyle) ~ ")") ^^ {
        case _ ~ _ ~ serie ~ _ ~ x ~ _ ~ normalize ~ None ~ _            => (normalize, ResultColumn.BarsStyle)
        case _ ~ _ ~ serie ~ _ ~ x ~ _ ~ normalize ~ Some(_ ~ style) ~ _ => (normalize, style)
      } // obsolete

      def seq[A](a: Parser[A]) = "[" ~> repsep(a, ",") <~ "]"

      val normalization = ("normalized" ^^^ true) | ("absolute" ^^^ false)
      val resultColumn = (("ResultColumn" ~> "(" ~> string ~ ("," ~> normalization) ~ ("," ~> plotStyle) <~ ")") ^^ {
        case coord ~ normalize ~ plotStyle => ResultColumn(coord.toCoord, normalize, plotStyle)
      }) | (("ResultColumn" ~> "(" ~> string ~ ("," ~> boolean) ~ ("," ~> seq(plot)) <~ ")") ^^ { // obsolete
        case coord ~ showColumn ~ plots => plots match {
          case Nil => ResultColumn(coord.toCoord)
          case p :: Nil => ResultColumn(coord.toCoord, p._1, p._2)
          case _ :: _ => ??? // Not possible
        }
      })

      def filtering(c: Coord): Parser[Filtering] =
        ("AllValues" ^^^ AllValues
          | ("SomeValues" ~> "(" ~> seq(string) <~ ")") ^^ {
            values => SomeValues(values map { v => configValueFromString(c, v) })
          })

      def use: Parser[Use] =
        ("TableColumn" ^^^ XColumn // deprecated
          | "XColumn" ^^^ XColumn
          | "SerieColumn" ^^^ SerieColumn
          | "Section" ^^^ Section
          | "FilterOnly" ^^^ FilterOnly)

      val indexColumn = ("IndexColumn" ~> "(" ~> string) into { coord =>
        (("," ~> filtering(coord.toCoord) <~ ",") ~ use <~ ")") ^^ {
          case filtering ~ use => IndexColumn(coord.toCoord, filtering, use)
        }
      }

      val listingSource: Parser[Listing.Source] = ("Source(" ~> string <~ ")") ^^ { s => Listing.sources.available.find(_.toString == s).getOrElse(Listing.sources.SimulationsMixes) }

      def listing: Parser[Listing] = ("Listing" ~> "(" ~> (listingSource <~ ",") ~ seq(indexColumn) ~ ("," ~> seq(resultColumn) <~ ")")) ^^ {
        case source ~ indexes ~ results => Listing(Report.this, Listing.sources.SimulationsMixes, indexes, results)
      } | ("Listing" ~> "(" ~> seq(indexColumn) ~ ("," ~> seq(resultColumn) <~ ")")) ^^ {
        case indexes ~ results => Listing(Report.this, Listing.sources.SimulationsMixes, indexes, results)
      }
    }

    def loadListing(file: java.io.File) = {
      val f = new java.io.FileReader(file)
      try parser.parseAll(parser.listing, f) match {
        case parser.Success(listing, _) => listing
        case x                          => sys.error("Loading " + file + ": " + x.toString)
      } finally f.close()
    }
    def loadListings = {
      def examples = TreeMap(("example": ListingId) -> Listing(Report.this, Listing.sources.SimulationsMixes,
        Seq(IndexColumn("num_cpus".toCoord, use = Section), IndexColumn("protocol".toCoord, use = SerieColumn), IndexColumn("benchmark".toCoord, use = XColumn)),
        Seq(ResultColumn("cycles".toCoord, normalize = true))))
      try {
        val files = new java.io.File(listingsDir).listFiles() match {
          case null =>
            println("Cannot read directory " + listingsDir)
            Array.empty[java.io.File]
          case l => l.filter(! _.getName.startsWith(".")) // ignore files starting with ".", like .gitignore
        }
        val loaded = files.map { f =>
          (f.getName: ListingId) -> (try {
            Some(loadListing(f))
          } catch {
            case e: Throwable =>
              println("Loading " + f + ": " + e)
              e.printStackTrace()
              None
          })
        } collect {
          case (id, Some(l)) => id -> l
        }
        if (loaded.isEmpty) examples else TreeMap.from(loaded)
      } catch {
        case e: Throwable =>
          println(e)
          e.printStackTrace()
          examples
      }
    }
  }
}
object Report {
  type Point = Gem5DataPoint

  def configVariations(sims: Iterable[Point]): SortedMap[Coord, Map[Any, Int]] = (TreeMap.empty[Coord, Seq[Any]] ++
    ((allCoords.values filter { _.isConfig }) map { k =>
      k -> (sims map (k.optFn(_) getOrElse "missing"))
    }).filter(!_._2.allEquals)).map {
      case (c, vs) => c -> (TreeMap.empty(c.ordering) ++ vs.countItemsSorted)
    }

  def configConstants(sims: Iterable[Point]): SortedMap[Coord, Any] = TreeMap.empty[Coord, Seq[Any]] ++
    (allCoords.values filter { _.isConfig } map { k =>
      k -> (sims map (k.optFn(_) getOrElse "missing"))
    }).filter(_._2.allEquals).map { case (k, v) => k -> v.headOption.getOrElse("empty") }
}

import Report.Point

case class Listing(parent: Report, source: Listing.Source, indexes: Seq[IndexColumn], results: Seq[ResultColumn]) {
  import Listing._

  override def toString = s"${(indexes map { _.toString }).mkString("(", ", ", ")")} → ${results.mkString("(", ", ", ")")}"

  val rowIndexColumns = indexes filter { _.use != IndexColumn.FilterOnly }
  def rowIndexOf(s: Point): RowIndex = rowIndexColumns map { i => i.coord.fn(s) }
  val sectionIndexColumns = indexes filter { _.use == Section }
  def sectionOf(s: Point): SectionIndex = sectionIndexColumns map { i => i.coord.fn(s) }
  val serieIndexColumns = indexes filter { _.use == SerieColumn }
  def serieOf(s: Point): SerieIndex = serieIndexColumns map { i => i.coord.fn(s) }
  val xIndexColumns = indexes filter { _.use == XColumn }
  def xOf(s: Point): XIndex = xIndexColumns map { i => i.coord.fn(s) }
  val rowIndexOrdering = SeqOrdering[Any, RowIndex](rowIndexColumns map { i => i.coord.ordering })
  val sectionIndexOrdering = SeqOrdering[Any, SectionIndex](sectionIndexColumns map { i => i.coord.ordering })
  val serieIndexOrdering = SeqOrdering[Any, SerieIndex](serieIndexColumns map { i => i.coord.ordering })
  val xIndexOrdering = SeqOrdering[Any, XIndex](xIndexColumns map { i => i.coord.ordering })

  println(s"Generating listing $this")

  val points: Seq[Point] = time("collecting points") { source.points(parent).toSeq }
  val filteredPoints: Seq[Point] = time("filtering points") {
    val filterIndexes = indexes.collect { case IndexColumn(c, SomeValues(vs), _) => (c, vs) }
    def filterFn(i: Point) = filterIndexes.forall { case (c, vs) => vs.contains(c.fn(i)) }
    points filter filterFn
  }

  type RowMap = Map[RowIndex, Row]
  val rows: RowMap = {
    val unsorted = time("grouping rows") { filteredPoints groupBy rowIndexOf}
    unsorted//time("sorting rows") { TreeMap.empty(rowIndexOrdering) ++ unsorted }
  }

  type SectionMap = SortedMap[SectionIndex, RowMap]
  val sectionRows: SectionMap = {
    val unsorted = time("grouping sections") { rows.groupBy { case (k, vs) => sectionOf(vs.head) } }
    time("sorting sections") { TreeMap.empty(sectionIndexOrdering) ++ unsorted }
  }

  implicit val ec: scala.concurrent.ExecutionContext = concurrent.ExecutionContext.global
  val sectionSeriePlotRows = ConcurrentCache[SectionIndex, SortedMap[SerieIndex, SortedMap[XIndex, Row]]] { idx =>
    time(s"indexing series in section ${idx.mkString("(", ",", ")")}") { TreeMap.empty(serieIndexOrdering) ++ sectionRows(idx).groupBy { case (k, vs) => serieOf(vs.head) }.view.mapValues { rows => TreeMap.empty(xIndexOrdering) ++ (rows.values map { row => xOf(row.head) -> row }) } }
  }

  val filteredValues = ConcurrentCache[Coord, Set[Any]] { c => TreeSet.empty(c.ordering) ++ (rows.values flatMap { ss => ss map { s => c.optFn(s) getOrElse "missing" } }) }
  time(s"collecting filtered values") {
    filteredValues.seed(indexes map (_.coord))
  }

  val unfilteredValuesCount = ConcurrentCache[Coord, Iterable[(Any, Int)]] { c => points.map(s => c.optFn(s) getOrElse "missing").countItemsSorted }
  time(s"counting values") {
    unfilteredValuesCount.seed(indexes map (_.coord))
  }

  def sectionIndexFromString(s: String): Option[SectionIndex] = sectionRows.keys.find(si => sectionIndexToString(si) == s)
  def sectionIndexToString(i: SectionIndex) = if (i.isEmpty) "-" else i.mkString(",")

  val plots = ConcurrentCache[(SectionIndex, ResultColumn), repscr.plots.Plot] {
    case (sec: SectionIndex, ResultColumn(yCoord, normalized, style)) =>
      import repscr.plots._
      val pd = PlotUtil.SimulationsPlotData(x = xIndexColumns.map(_.coord),
        y = yCoord,
        seriesC = serieIndexColumns.map(_.coord),
        points = sectionRows(sec).values.flatten,
        addAverage = true,
        normalize = if (normalized) PlotUtil.Normalization.Ratio else PlotUtil.Normalization.Absolute)
      val p: Plot = style match {
        case ResultColumn.BarsStyle if yCoord.stacked => new StackedBarPlot {
          legendOffsetY = 15
          categoriesLegendRows = 2
        }
        case ResultColumn.BarsStyle => new BarPlot
        case ResultColumn.LinesStyle => new LinePlot {
          useCategorizedXcoords = true
        }
      }

      p.yRangeMin = 0
      p.plotStyle = Plot.Style.ColorsDivergingSpectral11
      p.seriesLegend = serieIndexColumns.nonEmpty
      p.seriesLegendRows = 2

      pd.addToPlot(p)

      p.outputFiles(Format.pdf) = java.io.File.createTempFile("plot", ".pdf")
      p.outputFiles(Format.png) = java.io.File.createTempFile("plot", ".png")
      p.outputFiles(Format.tsv) = java.io.File.createTempFile("plot", ".tsv")
      // try to draw the plot, but do not throw if something fails
      time(s"generating plot $yCoord in section ${sec.mkString("(", ",", ")")}") { try p.draw() catch { case e: Throwable => println(s"Drawing $yCoord ${try p.outputFiles(Format.python) catch { case _ : NoSuchElementException => "(no py file created)"}}:"); e.printStackTrace() } }
      p
  }
  def plot(sec: SectionIndex, col: ResultColumn) = plots(sec, col)

  import repscr.points._

  def tsv: String = tsv(rowIndexColumns map { _.coord }, results map { _.coord }, rows.values.flatten)
  def tsv(sec: SectionIndex): String = {
    val usedSectionIndexes = sectionIndexColumns take sec.length
    tsv((rowIndexColumns diff usedSectionIndexes) map { _.coord }, results map { _.coord }, sectionRows(sec).values.flatten)
  }
  def tsv(indexColCoords: Seq[Coord], resultColCoords: Seq[Coord], sims: Iterable[Point]) = {
    // return either the value of a simulation or the average/concatenation of many simulations
    def rowValue(coord: Coord, rowSims: Row, missingValue: Any = 0.0) = {
      import repscr.points._
      def averageOrConcat(lany: Iterable[Any]): Any = lany.headOption match {
        case Some(s: String) => lany
        case Some(t: Iterable[Any]) => lany map { case t: Iterable[Any] => t case x => Seq(x) } map averageOrConcat
        case _ => (lany filter (v => !(v.isNaN || v.isInfinity)) reduceOption (_ + _) getOrElse missingValue) / lany.size
      }
      def stackedAverage(lstacks: Iterable[Iterable[(Any, Any)]]) = {
        val categories = lstacks.flatMap(_ map (_._1)).filterDuplicates
        val lmaps = lstacks map (_.toMap)
        categories map { c => c -> averageOrConcat(lmaps map (_.getOrElse(c, missingValue))) }
      }

      rowSims.size match {
        case 1 => coord.optFn(rowSims.head).getOrElse(missingValue)
        case _ =>
          if (coord.stacked) stackedAverage((rowSims map coord.optFn collect { case Some(v) => v }).asInstanceOf[Iterable[Iterable[(Any, Any)]]])
          else averageOrConcat(rowSims map coord.optFn collect { case Some(v) => v })
      }
    }
    case class Col(name: String, ordering: Ordering[Any], fn: Iterable[Point] => Any)
    def colsFromValue(coord: Coord, v: Any): Seq[Col] = {
      def iter(name: String, fn: Iterable[Point] => Any, v: Any): Seq[Col] = {
        // handle formatting and expand Coords that produce sequences (stacked coords usually) to use several columns 
        val cols: Seq[Col] = v.noCoordValue match {
          case _: Double | _: Float | _: Long | _: Int | _: String | _: Boolean =>
            Seq(Col(name, coord.ordering, s => fn(s).toString))
          case _: Option[_] =>
            Seq(Col(name, coord.ordering, s => fn(s) match {
              case None => ""
              case Some(x) => x.toString
              case x => x.toString
            }))
          case x: Vwe =>
            iter(name, { s => fn(s).value }, x.value) ++ iter(name + "_err", { s => fn(s).error }, x.error)
          case x: Iterable[_] =>
            x.toSeq.zipWithIndex flatMap {
              case (v, i) => v.noCoordValue match {
                case (tag, v) => iter(
                  name + "/" + tag,
                  { s => fn(s).asInstanceOf[Iterable[_]].find { case (t, v) => t == tag }.get.asInstanceOf[(Any, Any)]._2 },
                  v)
                case _ => iter(name + "/" + i, { s => (fn(s).asInstanceOf[Iterable[_]] drop i).headOption getOrElse "" }, v)
              }
            }
          case _ =>
            Seq(Col(name, coord.ordering, { s => s"${fn(s)}: ${fn(s).getClass}" }))
        }
        cols map { case Col(n, o, fn) => Col(n, o, { v => try fn(v) catch {case _: Throwable => ""} }) }
      }
      iter(coord.name, r => rowValue(coord, r), v)
    }

    val indexCols = indexColCoords flatMap { c =>
      sims flatMap { s => colsFromValue(c, rowValue(c, Seq(s))) } filterDuplicatesFunc { _.name }
    }

    def rowIndex(s: Point) = indexCols map (_.fn(Seq(s)))
    val rowIndexOrdering = SeqOrdering[Any, Iterable[Point]](indexCols map (_.ordering))
    val rows = TreeMap.empty(rowIndexOrdering) ++ (sims groupBy rowIndex)

    val resultCols = resultColCoords flatMap { c =>
      rows flatMap { r => colsFromValue(c, rowValue(c, r._2)) } filterDuplicatesFunc { _.name }
    }

    val cS = "\t"
    val lS = "\n"
    ((indexCols ++ resultCols) map { _.name } mkString cS) + lS + (
      rows map {
        case (index, sims) =>
          index ++ (resultCols map { c => c.fn(sims) }) mkString cS
      } mkString lS)
  }
}
object Listing {
  trait Source {
    def points(parent: Report): Iterable[Point]
  }
  object sources {
    case object Simulations extends Source {
      def points(p: Report) = p.simulations
    }
    case object SimulationsMixes extends Source {
      def points(p: Report) = p.mixes
    }
    val available: Seq[Source] = Seq(Simulations, SimulationsMixes)
  }

  case class IndexColumn(coord: Coord,
    filtering: Filtering = AllValues,
    use: Use = XColumn) {
    override def toString = filtering match {
      case AllValues          => coord.toString
      case SomeValues(Seq(v)) => s"$coord = $v"
      case SomeValues(v)      => s"$coord ∈ ${v.mkString("{", ", ", "}")}"
    }
  }
  object IndexColumn {
    sealed trait Filtering
    case object AllValues extends Filtering
    case class SomeValues(values: Seq[Any]) extends Filtering

    sealed trait Use
    case object Section extends Use
    case object SerieColumn extends Use
    case object XColumn extends Use
    case object FilterOnly extends Use
  }

  case class ResultColumn(coord: Coord,
    normalize: Boolean = false,
    plotStyle: ResultColumn.PlotStyle = ResultColumn.BarsStyle) {
    override def toString = coord.toString
    def optionsToString = s"${if (normalize) "normalized" else "absolute"}, ${plotStyle match {case ResultColumn.BarsStyle => "bars" case ResultColumn.LinesStyle => "lines"}}"
  }
  object ResultColumn {
    sealed trait PlotStyle
    case object BarsStyle extends PlotStyle
    case object LinesStyle extends PlotStyle
  }

  type RowIndex = Seq[Any]
  type SectionIndex = Seq[Any]
  type SerieIndex = Seq[Any]
  type XIndex = Seq[Any]
  type Row = Iterable[Point]
}
