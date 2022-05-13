package repscr

import repscr.gem5.Gem5Coords._
import repscr.gem5.Gem5DataPoint
import util.misc.SeqOrdering
import util.misc.RichIterable
import plots.{Plot, Serie, Normalization}

import scala.collection.SortedMap
import scala.collection.immutable.TreeMap

object PlotUtil {


  case class PlotData(x: Iterable[Coord], y: Coord, seriesC: Iterable[Coord], points: Iterable[Gem5DataPoint],
    addTotals: Boolean = false,
    name: String = "", namePrefix: String = "",
    normalize: Normalization = Normalization.Absolute,
    debug: Boolean = false) {

    type Point = Gem5DataPoint
    type SerieIndex = Iterable[Any]
    type XIndex = Iterable[Any]

    val serieIndexOrdering = SeqOrdering[Any, SerieIndex](seriesC map (_.ordering))
    val xIndexOrdering = SeqOrdering[Any, XIndex](x map (_.ordering))

    def serieOf(s: Point): SerieIndex = seriesC map (_.fn(s))
    def xOf(s: Point): XIndex = x map (_.fn(s))
    type Row = Iterable[Point]
    type SerieRows = SortedMap[XIndex, Row]
    val indexedRows: SortedMap[SerieIndex, SerieRows] = TreeMap.empty(serieIndexOrdering) ++ points.groupBy(serieOf).view.mapValues { rows =>
      TreeMap.empty(xIndexOrdering) ++ rows.groupBy(xOf)
    }
    type SerieValues = SortedMap[XIndex, Any]


    // return either the value of a simulation or the average/concatenation of many simulations
    def rowValue(rowSims: Row, missingValue: Any = 0.0) = {
      rowSims.size match {
        case 1 => y.optFn(rowSims.head).getOrElse(missingValue)
        case s =>
          def averageOrConcat(avg: Iterable[Any] => Any)(lany: Iterable[Any]): Any = lany.headOption match {
            case Some(s: String)        => lany // handle strings (e.g., file names, which can appear in the UI)
            case Some(t: Iterable[Any]) => ??? // TODO lany map { case t: Iterable[Any] => t case x => Seq(x) } map averageOrConcat
            case None                   => None
            case _                      => avg(lany)
          }
          println(s"Warning: multiple (${s}) values in a row for plot $name, averaging.")
          val values = rowSims.map(y.optFn(_).getOrElse(missingValue))
          if (y.stacked) stackedAverage(averageOrConcat(averageTotalFunction))(values.asInstanceOf[Iterable[Iterable[(Any, Any)]]])
          else averageOrConcat(averageTotalFunction)(values)
      }
    }

    def fileBaseName = namePrefix + (name match {
      case "" => s"${x mkString "+"}-${seriesC mkString "+"}-$y"
      case x  => x
    })

    def indexName(i: Iterable[Any]) = i map { case s: Iterable[Any] => s.mkString("[", ",", "]") case x => x.toString } mkString "," take 100 // limit to 100 characters to avoid generating huge graphs

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
      p.yAxisTitle = y.axisTitle // TODO: + (normalized)

      val absolutePlotValues = indexedRows.iterator.map { case (serieIndex, serieRows) =>
        indexName(serieIndex) -> serieRows.iterator.map { case (xindex, row) => indexName(xindex) -> rowValue(row) }.to(Iterable)
      }
      val plotValues = normalize match {
        case Normalization.Absolute => absolutePlotValues
        case _                      => ??? // TODO
      }
      plotValues foreach { case (serieName, serieRows) => p.add(serieName, serieRows) }

      if (addTotals) {
        if (y.stacked) {
          p.series.foreach { case Serie(serieName, serieData) =>
            val items = serieData map (_.y.asInstanceOf[Iterable[(Any, Any)]].toMap)
            val categories = items.flatMap(_.keys).filterDuplicates
            val avg = categories map { c => c -> averageTotalFunction(items.map(_.getOrElse(c, 0))) }
            p.addPoint(serieName, "Arithmetic\\nMean", avg, false, 2)
          }
        } else {
          p.series.foreach { case Serie(serieName, serieData) =>
            val avg = averageTotalFunction(serieData map (_.y))
            p.addPoint(serieName, "Arithmetic\\nMean", avg, false, 2)
          }
        }
        p.addSeparationLine(-1)
      }
    }
  }

  import points.CoordValue

  def filterNanNsAndInfs(lany: Iterable[Any]) = lany.map(CoordValue).filter { v => !(v.isNaN || v.isInfinity) }

  def averageTotalFunction(lany: Iterable[Any]): Any = {
    val l = filterNanNsAndInfs(lany)
    l.foldLeft(CoordValue(0))(_ + _) / l.size
  }

  def geometricMeanTotalFunction(lany: Iterable[Any]): Any = {
    val l = filterNanNsAndInfs(lany)
    l.foldLeft(CoordValue(1))(_ * _) pow (1.0 / l.size)
  }

  def stackedAverage(avg: Iterable[Any] => Any, missingValue: Any = 0.0)(lstacks: Iterable[Iterable[(Any, Any)]]) = {
    val categories = lstacks.flatMap(_ map (_._1)).filterDuplicates
    val lmaps = lstacks map (_.toMap)
    categories map { c => c -> avg(lmaps map (_.getOrElse(c, missingValue))) }
  }
}