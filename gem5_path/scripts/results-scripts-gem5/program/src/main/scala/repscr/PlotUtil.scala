package repscr

import repscr.gem5.Gem5Coords._
import repscr.gem5.Gem5DataPoint
import util.misc.SeqOrdering
import util.misc.RichIterable
import plots.{Plot, Serie}

import scala.collection.immutable.TreeMap

object PlotUtil {
  case class PlotData(x: Seq[Coord], y: Coord, seriesC: Seq[Coord], points: Seq[Gem5DataPoint],
    addTotals: Boolean = false,
    name: String = "", namePrefix: String = "",
    yMultipleValues: Option[Seq[Any] => Any] = None,
    debug: Boolean = false) {
    type Point = Gem5DataPoint
    type SerieIndex = Seq[Any]
    type XIndex = Seq[Any]
    def indexName(i: Seq[Any]) = i map { case s: Seq[Any] => s.mkString("[", ",", "]") case x => x.toString } mkString "," take 100 // limit to 100 characters to avoid generating huge graphs

    val serieIndexOrdering = SeqOrdering[Any, SerieIndex](seriesC map (_.ordering))
    val xIndexOrdering = SeqOrdering[Any, XIndex](x map (_.ordering))

    def serieOf(s: Point): SerieIndex = seriesC map (_.fn(s))
    def xOf(s: Point): XIndex = x map (_.fn(s))
    def yOf(s: Point): Any = y.fn(s)

    def xAxisTitle = x map (_.axisTitle) mkString ","
    def yAxisTitle = y.axisTitle

    def fileBaseName = namePrefix + (name match {
      case "" => s"${x mkString "+"}-${seriesC mkString "+"}-$y"
      case x => x
    })

    def yMultipleValuesFn = yMultipleValues.getOrElse({ s: Seq[Any] =>
      println(s"Warning: multiple (${s.length}) values in a row for plot $name, averaging.")
      averageTotalFunction(s)
    })

    val seriesSims = TreeMap.empty(serieIndexOrdering) ++ points.groupBy(serieOf).view.mapValues { rows =>
      TreeMap.empty(xIndexOrdering) ++ (rows.groupBy(xOf).view.mapValues { sims =>
        if (sims.length == 1) (yOf(sims.head), sims)
        else {
          println("Multiple values configuration variations:")
          for ((k, v) <- Gem5DataPoint.findConfigVariations(sims)) println(s"  ${k}: ${v.countItemsSorted.mkString(",")}")
          (yMultipleValuesFn(sims.map(yOf)), sims)
        }
      })
    }

    if (debug) {
      println(s"PlotData: $name")
      seriesSims.foreach { case (serieIndex, seriePoints) =>
        println(s"  serie: ${indexName(serieIndex)}")
        seriePoints.foreach { case (xindex, (yvalue, sims)) =>
          println(s"    ${xindex.mkString(",")} -> $yvalue   (${sims.length}: ${sims.map(_.benchmarkName).sorted.mkString("[", ",", "]")})")
        }
      }
    }
    val seriesPoints = seriesSims.toSeq.map {
      case (serie, rows) => indexName(serie) -> rows.toSeq.map {
        case (rx, (ry, sims)) => (if (rx.length != 1) indexName(rx) else rx.head) -> ry
      }
    }

    def addToPlot(p: Plot): Unit = {
      seriesPoints foreach { case (serieName, serieRows) =>
        p.add(serieName, serieRows)
      }

      if (addTotals) {
        if (y.stacked) {
          p.series.foreach { case Serie(serieName, serieData) =>
            val items = serieData map (_.y.asInstanceOf[Iterable[(Any, Any)]].toMap)
            val categories = items.flatMap(_.keys).filterDuplicates
            val avg = categories map { c => c -> averageTotalFunction(items map (_.getOrElse(c, 0))) }
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

  def averageTotalFunction(lany: Iterable[Any]) = {
    val l = lany.map(CoordValue).filter { v => !(v.isNaN || v.isInfinity) }
    l.foldLeft(CoordValue(0))(_ + _) / l.size
  }
  def geometricMeanTotalFunction(lany: Iterable[Any]) = {
    val l = lany.map(CoordValue).filter { v => !(v.isNaN || v.isInfinity) }
    l.foldLeft(CoordValue(1))(_ * _) pow (1.0 / l.size)
  }
  def maxTotalFunction(lany: Iterable[Any]) = lany.map(CoordValue).filter { v => !(v.isNaN || v.isInfinity) }.foldLeft(CoordValue(0))(_ max _)
}