package repscr

import repscr.gem5.Gem5Coords._
import util.misc.SeqOrdering
import util.misc.RichIterable
import plots.Plot

import scala.collection.SortedMap
import scala.collection.immutable.TreeMap

object PlotUtil {
  case class Point(x: Any, y: Any)
  case class Serie(label: String, data: Iterable[Point]) {
    def addTotals(totals: Iterable[(String, TotalFunctions.TotalFunction)], stacked: Boolean): Serie = Serie(label, data ++ totals.map {
      case (label, tf) =>
        if (stacked) Point(label, TotalFunctions.stackedAverage(tf)(data.map(_.y.asInstanceOf[Iterable[(Any, Any)]])))
        else Point(label, tf(data.map(_.y)))
    })
  }
  case class PlotData(series: Iterable[Serie]) {
    import points._

    def normalized(normalizationMode: Normalization): PlotData =
      if (series.isEmpty || normalizationMode == Normalization.Absolute) this
      else normalized(normalizationMode, series.head)

    def normalized(normalizationMode: Normalization, base: Serie, skipBase: Boolean = false): PlotData = {
      val baseValues = base.data.view.map(p => p.x -> p.y).map {
        case (x, y: Iterable[_]) => x -> y.asInstanceOf[Iterable[(Any, Any)]].map(_._2.value).sum
        case (x, y) => x -> y.value
      }.toMap
      def normalizePoint(p: Point): Option[Point] = p match {
        case Point(x, y: Iterable[_]) => baseValues get x map { v => Point(x, y.asInstanceOf[Iterable[(Any, Any)]] map { case (a, b) => a -> normalizationMode(b, v) }) }
        case Point(x, y) => baseValues get x map { v => Point(x, normalizationMode(y, v)) }
      } // will be None if there is no matching base
      PlotData(series.filter(!skipBase || _ != base).map { case Serie(l, d) => Serie(l, d flatMap normalizePoint) })
    }

    def addTotals(totals: Iterable[(String, TotalFunctions.TotalFunction)], stacked: Boolean): PlotData = PlotData(series.map(_.addTotals(totals, stacked)))
  }

  sealed trait Normalization {
    import points._

    def apply(value: Any, baseValue: Any): Any = this match {
      case Normalization.Absolute => value
      case Normalization.Ratio => value normalize baseValue
      case Normalization.RatioPerCent => (value normalize baseValue) * 100
      case Normalization.Increase => (value normalize baseValue) - 1
      case Normalization.IncreasePerCent => ((value normalize baseValue) - 1) * 100
      case Normalization.Decrease => (baseValue normalize value) - 1
      case Normalization.DecreasePerCent => ((baseValue normalize value) - 1) * 100
      case Normalization.Speedup => baseValue normalize value
    }
  }
  object Normalization {
    case object Absolute extends Normalization
    case object Ratio extends Normalization
    case object RatioPerCent extends Normalization
    case object Increase extends Normalization
    case object IncreasePerCent extends Normalization
    case object Decrease extends Normalization
    case object DecreasePerCent extends Normalization
    case object Speedup extends Normalization
  }

  case class SimulationsPlotData(
    x: Iterable[Coord], y: Coord, seriesC: Iterable[Coord], points: Iterable[DataPointType],
    addAverage: Boolean = false,
    name: String = "", namePrefix: String = "",
    normalize: Normalization = Normalization.Absolute,
    debug: Boolean = false) {

    type SerieIndex = Iterable[Any]
    type XIndex = Iterable[Any]

    val serieIndexOrdering = SeqOrdering[Any, SerieIndex](seriesC map (_.ordering))
    val xIndexOrdering = SeqOrdering[Any, XIndex](x map (_.ordering))

    def serieOf(s: DataPointType): SerieIndex = seriesC map (_.fn(s))
    def xOf(s: DataPointType): XIndex = x map (_.fn(s))
    type Row = Iterable[DataPointType]
    type SerieRows = SortedMap[XIndex, Row]
    val indexedRows: SortedMap[SerieIndex, SerieRows] = TreeMap.empty(serieIndexOrdering) ++ points.groupBy(serieOf).view.mapValues { rows =>
      TreeMap.empty(xIndexOrdering) ++ rows.groupBy(xOf)
    }

    // return either the value of a simulation or the average/concatenation of many simulations
    def rowValue(rowSims: Row, missingValue: Any = 0.0) = {
      rowSims.size match {
        case 1 => y.optFn(rowSims.head).getOrElse(missingValue)
        case s =>
          def averageOrConcat(avg: Iterable[Any] => Any)(lany: Iterable[Any]): Any = lany.headOption match {
            case Some(s: String) => lany // handle strings (e.g., file names, which can appear in the UI or TSV files)
            case Some(t: Iterable[_]) => ??? // TODO lany map { case t: Iterable[Any] => t case x => Seq(x) } map averageOrConcat
            case None => None
            case _ => avg(lany)
          }
          println(s"Warning: multiple (${s}) values in a row for plot $name, averaging.")
          val values = rowSims.map(y.optFn(_).getOrElse(missingValue))
          if (y.stacked) TotalFunctions.stackedAverage(averageOrConcat(TotalFunctions.averageTotalFunction))(values.asInstanceOf[Iterable[Iterable[(Any, Any)]]])
          else averageOrConcat(TotalFunctions.averageTotalFunction)(values)
      }
    }

    def fileBaseName = namePrefix + (name match {
      case "" => s"${x mkString "+"}-${seriesC mkString "+"}-$y"
      case x => x
    })

    def indexName(i: Iterable[Any]) = i map { case s: Iterable[Any] => s.mkString("[", ",", "]") case x => x.toString } mkString "," take 100 // limit to 100 characters to avoid generating huge graphs

    val plotData = {
      val absolutePlotValues = PlotData(
        indexedRows.view.map { case (serieIndex, serieRows) =>
          Serie(indexName(serieIndex), serieRows.view.map { case (xindex, row) => Point(indexName(xindex), rowValue(row)) }.toSeq)
        }.toSeq)
      val normalizedPlotValues = absolutePlotValues.normalized(normalize)
      if (addAverage) {
        normalizedPlotValues.addTotals(Seq(("Arithmetic\\nMean", TotalFunctions.averageTotalFunction)), y.stacked)
      } else normalizedPlotValues
    }

    if (debug) {
      println(s"PlotData: $name")
      indexedRows.foreach { case (serieIndex, serieRows) =>
        println(s"  serie: ${indexName(serieIndex)}")
        serieRows.foreach { case (xindex, sims) =>
          println(s"    ${xindex.mkString(",")} -> ${rowValue(sims)}   (${sims.size}: ${sims.map(_.benchmarkName).toSeq.sorted.mkString("[", ",", "]")})")
        }
      }
    }

    def addToPlot(p: Plot): Unit = {
      p.xAxisTitle = x map (_.axisTitle) mkString ","
      p.yAxisTitle = y.axisTitle + (normalize match {
        case Normalization.Ratio => " (normalized)"
        case _ => "" // TODO: speedup, increase…
      })
      p match {
        case p: plots.StackedBarPlot =>
          p.categoriesOrder = Some(y.ordering.lt)
        case _ =>
      }
      p.data = plotData
      if (addAverage) {
        p.addSeparationLine(-1)
      }
    }
  }

  object TotalFunctions {
    type TotalFunction = Iterable[Any] => Any

    import points.CoordValue

    def averageTotalFunction(lany: Iterable[Any]): Any = {
      val l = filterNanNsAndInfs(lany)
      l.foldLeft(CoordValue(0))(_ + _) / l.size
    }

    def geometricMeanTotalFunction(lany: Iterable[Any]): Any = {
      val l = filterNanNsAndInfs(lany)
      l.foldLeft(CoordValue(1))(_ * _) pow (1.0 / l.size)
    }

    def stackedAverage(avg: Iterable[Any] => Any, missingValue: Any = 0.0)(lstacks: Iterable[Iterable[(Any, Any)]]) = {
      val categories = lstacks.flatMap(_.map(_._1)).filterDuplicates
      val lmaps = lstacks.map(_.toMap)
      categories map { c => c -> avg(lmaps.map(_.getOrElse(c, missingValue))) }
    }

    private def filterNanNsAndInfs(lany: Iterable[Any]) = lany.map(CoordValue).filter { v => !(v.isNaN || v.isInfinity) }
  }
}