package scripts

import repscr.gem5.Gem5Coords._
import repscr.gem5.{Gem5DataPoint, SimulationMix}
import repscr.plots.{BarPlot, BarPlotCommon, Format, Normalization, Plot, Serie, StackedBarPlot}
import util.misc._

import java.io.File
import scala.collection.immutable.TreeMap

object Plots202204CostEffective extends App with PlotScript {
  object PlotUtils {
    trait DefaultPlotOptions { self: BarPlotCommon =>
      yRangeMin = 0
      xAxisLabelFormat = "/hR/vM/a15{}%s"
      yAxisLabelFormat = "%5.1f"
      width = 90
      height = 60
      fontSize = 14
      seriesLegend = true
      seriesLegendRows = 1
      outOfRangeLabelScale = 0.8
      outOfRangeLabelYoffsetInc = outOfRangeLabelYoffsetInc + 3
      barWidth = 15
      plotStyle = Plot.Style.ColorsDivergingSpectral11
      normalization = Normalization.Ratio
    }

    case class PlotData(x: Seq[Coord], y: Coord, seriesC: Seq[Coord], points: Seq[Gem5DataPoint],
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
        Plot.averageTotalFunction(s)
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

      def addToPlot(p: Plot): Unit = seriesPoints foreach { case (serieName, serieRows) => p.add(serieName, serieRows) }
    }

    class BarPlotDefault(val data: PlotData) extends BarPlot with DefaultPlotOptions {
      outputFiles(Format.pdf) = new File(s"$outdir/${data.fileBaseName}.pdf")
      outputFiles(Format.tsv) = new File(s"$outdir/${data.fileBaseName}.tsv")
      xAxisTitle = data.xAxisTitle
      yAxisTitle = data.yAxisTitle
      //pointsOrder = Some(data.x.ordering.lt)
      //seriesOrder = Some(data.seriesC.ordering.lt)
      totalPointFunction = None
      legendOffsetX = -5
      legendOffsetY = 5

      data.addToPlot(this)

      def addTotals(): Unit = {
        assert(seriesOrder.isEmpty)
        assert(pointsOrder.isEmpty)

        sortData()

        series foreach { case Serie(serieName, serieData) =>
          val avg = Plot.averageTotalFunction(serieData map (_.y))
          addPoint(serieName, "Arithmetic\\nMean", avg, false, 2)
        }
        addSeparationLine(-1)
      }
    }

    class StackedBarPlotDefault(val data: PlotData) extends StackedBarPlot with DefaultPlotOptions {
      outputFiles(Format.pdf) = new File(s"$outdir/${data.fileBaseName}.pdf")
      outputFiles(Format.tsv) = new File(s"$outdir/${data.fileBaseName}.tsv")
      xAxisTitle = data.xAxisTitle
      yAxisTitle = data.yAxisTitle
      //pointsOrder = Some(x.ordering.lt)
      //seriesOrder = Some(seriesC.ordering.lt)
      categoriesOrder = Some(data.y.ordering.lt)
      totalPointFunction = None
      seriesLegend = true
      categoriesLegendRows = 1
      legendOffsetX = -5
      legendOffsetY = 13

      data.addToPlot(this)

      def addTotals(): Unit = {
        sortData()

        series foreach { case Serie(serieName, serieData) =>
          val items = serieData map (_.y.asInstanceOf[Iterable[(Any, Any)]].toMap)
          val categories = items.flatMap(_.keys).filterDuplicates
          val avg = categories map { c => c -> Plot.averageTotalFunction(items map (_.getOrElse(c, 0))) }
          addPoint(serieName, "Arithmetic\\nMean", avg, false, 2)
        }

        val added = 1
        if (added > 0) addSeparationLine(-added)
      }
    }


  }

  "config_huawei".toCoord.derivedCoord("config", identity) // TODO: Move from Gem5Coords

  "htm_transaction_abort_cause".toCoord.derivedCoord("htm_transaction_abort_cause_grouped", m => regroupMap[String, Map[String, Any]](m.asMap[String, Any]) {
    case "memory_conflict" | "memory_conflict_fallbacklock" | "lsq_conflict" | "memory_conflict_staledata" | "memory_conflict_falsesharing" => "conflict"
    case "interrupt" | "exception" => "exception"
    case "transaction_size_wset" | "transaction_size_l1priv" | "transaction_size_wrongcache" | "transaction_size_rset" => "size"
    case "explicit" => "explicit"
    case x => println(s"XXXX   $x"); x
  }, ordering = dynamicOrdering("conflict", "size", "interrupt", "explicit"))

  "htm_transaction_abort_cause".toCoord.derivedCoord("htm_transaction_abort_cause_grouped_sizes", m => regroupMap[String, Map[String, Any]](m.asMap[String, Any]) {
    case "memory_conflict" | "memory_conflict_fallbacklock" | "lsq_conflict" | "memory_conflict_staledata" | "memory_conflict_falsesharing" => "conflict"
    case "interrupt" | "exception" => "exception"
    case "transaction_size_wset" => "size_l1wset"
    case "transaction_size_l1priv" => "size_l2priv"
    case "transaction_size_wrongcache" => "size_other"
    case "transaction_size_rset" => "size_l1rset"
    case "explicit" => "explicit"
    case x => println(s"XXXX   $x"); x
  }, ordering = dynamicOrdering("conflict", "size", "interrupt", "explicit"))

  "htm_transaction_abort_cause".toCoord.derivedCoord("htm_transaction_abort_cause_grouped_conflicts", m => regroupMap[String, Map[String, Any]](m.asMap[String, Any]) {
    case "memory_conflict_fallbacklock" => "fbacklock"
    case "memory_conflict" | "lsq_conflict" | "memory_conflict_staledata" | "memory_conflict_falsesharing" => "conflict"
    case "interrupt" | "exception" => "exception"
    case "transaction_size_wset" | "transaction_size_l1priv" | "transaction_size_wrongcache" | "transaction_size_rset" => "size"
    case "explicit" => "explicit"
    case x => println(s"XXXX   $x"); x
  }, ordering = dynamicOrdering("conflict", "size", "interrupt", "explicit"))


  implicit class dataPointAccessors(s: Gem5DataPoint) {
    import repscr.points.CoordValue

    def config = "config".toCoord.fn(s).toString
    def cycles_ticks = "cycles_ticks".toCoord.fn(s).toVwe
  }


  val points = {
    def acceptable(m: SimulationMix) = m.cycles_ticks.relativeError < .15
    def limitVariation(m: SimulationMix): SimulationMix =
    //println(s"${m.simulations.size} ${m.benchmarkName} ${m.num_cpus}p ${m.cycles_ticks.relativeError} ${m.simulations.toSeq.sortBy(_.cycles_ticks).map(_.cycles_ticks.value.toLong).mkString(" ")}")
      if (acceptable(m)) m
      else limitVariation(new SimulationMix(m.simulations.toSeq.sortBy(_.cycles_ticks).dropRight(1)))

    mixes.map(limitVariation)
  }

  import PlotUtils._

  val allPlots = collection.mutable.Buffer.empty[Plot]

  allPlots +=
  new BarPlotDefault(PlotData(name = "global",
    seriesC = Seq("config".toCoord, "num_cpus".toCoord),
    y = "cycles_ticks".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points)
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Time (normalized)"
    seriesLegendRows = 3
    width = 400
  }

  allPlots +=
  new BarPlotDefault(PlotData(name = "global-1thread",
    seriesC = Seq("config".toCoord),
    y = "cycles_ticks".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(_.num_cpus == 1))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Time (normalized)"
    seriesLegendRows = 3
    width = 180
  }

  allPlots +=
  new BarPlotDefault(PlotData(name = "global-16thread",
    seriesC = Seq("config".toCoord),
    y = "cycles_ticks".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(_.num_cpus == 16))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Time (normalized)"
    seriesLegendRows = 3
    width = 180
  }

  allPlots +=
  new BarPlotDefault(PlotData(name = "plot1",
    seriesC = Seq("config".toCoord),
    y = "cycles_ticks".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 1
                                && Set("locks", "base_nopf").contains(s.config)
                                && Set("intruder", "genome", "kmeans-h", "ssca2", "vacation-h", "yada").contains(s.benchmarkName)))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Time (normalized)"
  }

  allPlots +=
  new StackedBarPlotDefault((PlotData(name = "plot2a",
    seriesC = Seq("config".toCoord),
    y = "htm_transaction_abort_cause_grouped".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 1
                                && Set("base_nopf", "base").contains(s.config)
                                && Set("vacation-h", "intruder", "yada").contains(s.benchmarkName))))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Abort count (normalized)"
  }

  allPlots +=
  new BarPlotDefault(PlotData(name = "plot2b",
    seriesC = Seq("config".toCoord),
    y = "cycles_ticks".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 16
                                && Set("base_nopf", "base").contains(s.config)
                                && Set("vacation-h", "intruder", "yada").contains(s.benchmarkName)))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Time (normalized)"
  }


  allPlots +=
  new StackedBarPlotDefault(PlotData(name = "plot3a",
    seriesC = Seq("config".toCoord),
    y = "htm_transaction_abort_cause_grouped_sizes".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 1
                                && Set("base", "l2rs", "l3rs", "lxrs").contains(s.config)
                                && Set("vacation-h", "yada", "intruder").contains(s.benchmarkName)))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Abort count (normalized)"
    categoriesLegendRows = 2
  }

  allPlots +=
  new BarPlotDefault(PlotData(name = "plot3b",
    seriesC = Seq("config".toCoord),
    y = "cycles_ticks".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 16
                                && Set("base", "l2rs", "l3rs", "lxrs").contains(s.config)
                                && Set("vacation-h", "yada", "intruder").contains(s.benchmarkName)))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Time (normalized)"
  }

  allPlots +=
  new StackedBarPlotDefault(PlotData(name = "plot4a",
    seriesC = Seq("config".toCoord),
    y = "htm_transaction_abort_cause_grouped_sizes".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 1
                                && Set("l3rs", "l3rs_l1rpl").contains(s.config)
                                && Set("vacation-h", "yada", "intruder").contains(s.benchmarkName)))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Abort count (normalized)"
  }

  allPlots +=
  new BarPlotDefault(PlotData(name = "plot4b",
    seriesC = Seq("config".toCoord),
    y = "cycles_ticks".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 1
                                && Set("l3rs", "l3rs_l1rpl").contains(s.config)
                                && Set("vacation-h", "yada", "intruder").contains(s.benchmarkName)))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Time (normalized)"
  }

  allPlots +=
  new StackedBarPlotDefault(PlotData(name = "plot5a",
    seriesC = Seq("config".toCoord),
    y = "htm_transaction_abort_cause_grouped_conflicts".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 16
                                && Set("l3rs_l1rpl", "l3rs_l1rpl_reqstallb", "l3rs_l1rpl_reqstallh").contains(s.config)
                                && Set("kmeans-h", "yada", "intruder").contains(s.benchmarkName)))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Abort count (normalized)"
    seriesLegendRows = 2
    legendOffsetY = 17
  }

  allPlots +=
  new BarPlotDefault(PlotData(name = "plot5b",
    seriesC = Seq("config".toCoord),
    y = "cycles_ticks".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 16
                                && Set("l3rs_l1rpl", "l3rs_l1rpl_reqstallb", "l3rs_l1rpl_reqstallh").contains(s.config)
                                && Set("kmeans-h", "yada", "intruder").contains(s.benchmarkName)))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Time (normalized)"
    seriesLegendRows = 2
  }

  allPlots +=
  new StackedBarPlotDefault(PlotData(name = "plot6a",
    seriesC = Seq("config".toCoord),
    y = "htm_transaction_abort_cause_grouped".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 16
                                && Set("l3rs_l1rpl_reqstallh", "l3rs_l1rpl_reqstallh_precrs").contains(s.config)))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Abort count (normalized)"
    seriesLegendRows = 2
    legendOffsetY = 17
  }

  allPlots +=
  new BarPlotDefault(PlotData(name = "plot6b",
    seriesC = Seq("config".toCoord),
    y = "cycles_ticks".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 16
                                && Set("l3rs_l1rpl_reqstallh", "l3rs_l1rpl_reqstallh_precrs").contains(s.config)))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Time (normalized)"
    seriesLegendRows = 2
  }

  allPlots +=
  new StackedBarPlotDefault(PlotData(name = "plot7a",
    seriesC = Seq("config".toCoord),
    y = "htm_transaction_abort_cause_grouped".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 16
                                && Set("l3rs_l1rpl_reqstallh_precrs", "l3rs_l1rpl_lazycd").contains(s.config)))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Abort count (normalized)"
    seriesLegendRows = 2
  }

  allPlots +=
  new BarPlotDefault(PlotData(name = "plot7b",
    seriesC = Seq("config".toCoord),
    y = "cycles_ticks".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 16
                                && Set("l3rs_l1rpl_reqstallh_precrs", "l3rs_l1rpl_lazycd").contains(s.config)))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Time (normalized)"
    seriesLegendRows = 2
  }

  allPlots +=
  new BarPlotDefault(PlotData(name = "plot8b",
    seriesC = Seq("config".toCoord),
    y = "cycles_ticks".toCoord,
    x = Seq("benchmark_name".toCoord),
    points = points.filter(s => s.num_cpus == 16
                                && Set("base", "l3rs_l1rpl_reqstallh_precrs", "l3rs_l1rpl_lazycd", "lxrs_l1rpl_reqstallh_precrs_log").contains(s.config)))
  ) {
    normalization = Normalization.Ratio
    yAxisTitle = "Time (normalized)"
    seriesLegendRows = 2
    width = 150
  }


  createDirs(outdir)
  plotUtil.plotWithProgress(allPlots)
}
