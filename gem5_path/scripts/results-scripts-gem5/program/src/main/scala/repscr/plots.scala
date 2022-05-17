package repscr

import scala.language.implicitConversions
import language.postfixOps
import java.io.File
import util.misc._
import points._
import math._
import scala.collection.mutable.{Map => MMap}

object plots {
  sealed trait Format
  object Format {
    case object eps extends Format
    case object pdf extends Format
    case object python extends Format
    case object png extends Format
    case object svg extends Format
    case object tsv extends Format
  }

  import PlotUtil.Point
  import PlotUtil.Serie

  abstract class Plot {
    var xAxisTitle: String = ""
    var yAxisTitle: String = ""
    var xAxisLabelFormat: String = "/hR/vT/a30{}%s"
    var xAxisHide = false
    var yAxisLabelFormat: String = "%5g"
    var width: Int = 0 // 0 -> automatic
    var height: Int = 100
    var fontSize: Double = 10.0
    var seriesLegend: Boolean = true
    var seriesLegendRows: Int = 1
    var legendOffsetX: Int = 5
    var legendOffsetY: Int = 5
    var title: String = ""
    val outputFiles = MMap.empty[Format, File]

    var plotStyle: Plot.Style = Plot.Style.Colors1NoDashes

    protected var _data: PlotUtil.PlotData = PlotUtil.PlotData(Seq.empty[Serie])
    def data = _data
    def data_=(data: PlotUtil.PlotData): Unit = {
      _data = data
    }
    def series: Iterable[Serie] = _data.series

    protected val separationLines = collection.mutable.ArrayBuffer.empty[Int]
    def addSeparationLine(xCoordToTheLeft: Int) = // if xCoordToTheLeft negative, it starts counting from the right
      separationLines += xCoordToTheLeft

    def getXValues: Iterable[Any] = series flatMap (_.data) map (_.x)
    def getYValues: Iterable[Any] = series flatMap (_.data) map (_.y)
    def getValidTotalYValues: Iterable[Double] = getYValues filter { x => !x.isInfinity && !x.isNaN } map { _.value }
    def getYValuesRangeSize: Double = if (getValidTotalYValues.isEmpty) 0 else abs(getValidTotalYValues.max - getValidTotalYValues.min) match {case x if x.isInfinite => Double.MaxValue case x => x}

    var _yRangeMin: Option[Double] = None
    def yRangeMin_=(v: Double): Unit = {require(!v.isInfinite); _yRangeMin = Some(v) }
    def yRangeMin = _yRangeMin getOrElse { if (getValidTotalYValues.isEmpty) 0 else getValidTotalYValues.min - 0.05 * (getYValuesRangeSize max 0.000001) } ensuring { v => !v.isInfinite }

    var _yRangeMax: Option[Double] = None
    def yRangeMax_=(v: Double): Unit = { require(!v.isInfinite); _yRangeMax = Some(v) }
    def yRangeMax = _yRangeMax getOrElse { if (getValidTotalYValues.isEmpty) 0 + 0.05 * 0.000001 else (getValidTotalYValues.max max yRangeMin) + 0.05 * (getYValuesRangeSize max 0.000001) } ensuring { v => !v.isInfinite && v > yRangeMin }

    var _yGridInterval: Option[Double] = None
    def yGridInterval_=(v: Double): Unit = { _yGridInterval = Some(v) }
    def yGridInterval = _yGridInterval getOrElse {
      val rangeSize = abs(yRangeMax - yRangeMin)
      if (rangeSize > 0) {
        val maxstep = rangeSize / 7.01 // At least 8, maximum 15 divisions
        val ndigits = floor(log(rangeSize) / log(10))
        val minstep = pow(10, ndigits) / 16
        ((Seq(2, 4, 8, 16) map (_ * minstep) filter (_ < maxstep)) :+ minstep).max
      } else {
        1
      }
    }

    def xCoordsArray = getXValues.filterDuplicates map (_.toString)
    def autoWidth =
      series.size * xCoordsArray.size * 5

    def draw(): Unit = {
      var drawn = Set.empty[Format]
      def draw(fmt: Format): Unit = {
        if (!drawn(fmt)) {
          val ext = fmt match {
            case Format.python => ".py"
            case Format.pdf    => ".pdf"
            case Format.eps    => ".eps"
            case Format.png    => ".png"
            case Format.svg    => ".svg"
            case Format.tsv    => ".tsv"
          }
          if (!outputFiles.contains(fmt)) {
            outputFiles(fmt) = File.createTempFile("plot", ext)
          }
          val file = outputFiles(fmt)
          fmt match {
            case Format.python =>
              printToFile(file) { printCode() }
              file.setExecutable(true)
            case Format.pdf | Format.eps =>
              draw(Format.python)
              // hack for problem described at http://adam-dev-blog.blogspot.com.es/2013/05/a-fork-exec-surprise.html
              var done = false
              var tries = 0
              while (!done && tries < 100) {
                try {
                  val p = new ProcessBuilder(outputFiles(Format.python).getCanonicalPath, fmt match {
                    case Format.pdf => "--format=pdf"
                    case Format.eps => "--format=eps"
                    case _          => throw new RuntimeException("unreachable")
                  }).start
                  p.getOutputStream.close()
                  import concurrent.ExecutionContext.Implicits.global
                  import concurrent.duration.Duration
                  import concurrent.Future
                  import concurrent.Await
                  val errors = Future { p.getErrorStream.readAsString() }
                  p.getInputStream copyToFile file
                  if (p.waitFor != 0) {
                    Console.err.println(Await.result(errors, Duration.Inf))
                    sys.error("Some error drawing " + outputFiles(Format.python))
                  }
                  done = true
                } catch {
                  case e: Throwable =>
                    tries = tries + 1
                    println(s"Fork&exec race try " + tries)
                    println(e)
                    Thread.sleep(100) // wait to let forked processes close the fd
                }
              }
            case Format.png | Format.svg =>
              draw(Format.pdf)
              val q = new ProcessBuilder("/usr/bin/convert", "-density", "125", outputFiles(Format.pdf).getCanonicalPath, (fmt match {
                case Format.png => "png:"
                case Format.svg => "svg:"
                case _ => throw new RuntimeException("Unreachable")
              }) + file).start
              q.getOutputStream.close()
              q.getInputStream.readAndDiscard()
              if (q.waitFor != 0) sys.error(s"Some error converting from pdf ${outputFiles(Format.pdf).getCanonicalPath} to $fmt $file")
            case Format.tsv =>
              printToFile(file) { printTsv() }
          }
          drawn += fmt
        }
      }
      for (f <- outputFiles.keys.toList) draw(f)
    }

    protected def printCode(): Unit

    protected def printTsv(): Unit

    protected def printPyChartCommonCode(): Unit = {
      if (width == 0) width = autoWidth

      print(s"""|#!/usr/bin/python2
                |# -*- coding: utf-8 -*-
                |
                |from pychart import *
                |import pychart.line_style
                |import pychart.canvas
                |
                |def mm_to_pt(x):
                |    return x * 72 / 25.4
                |
                |import pychart.theme
                |
                |theme.default_font_size = $fontSize
                |theme.title = '$title'
                |theme.use_color = ${if (plotStyle.useColors) "True" else "False"}
                |theme.get_options()
                |
                |# series.size, xCoordsArray.size, (series map (_.data.size))
                |# ${series.size}, ${xCoordsArray.size}, ${series.map(_.data.size).mkString("[",",","]")}
                |
                |area_size = (mm_to_pt($width), mm_to_pt($height))
                |legend_loc = (mm_to_pt($legendOffsetX), area_size[1] + mm_to_pt($legendOffsetY))
                |
                |def rgb(rx, gx, bx):
                |    return fill_style.Plain(bgcolor=color.T(r = rx / 256.0, g = gx / 256.0, b = bx / 256.0))
                |
                |""".stripMargin)

      print(s"""|my_fill_style = ${plotStyle.pyFillStyle}
                |
                |my_fill_style_len = len(my_fill_style)
                |""".stripMargin)

      print(s"""|my_line_style = ${plotStyle.pyLineStyle}
                |
                |my_line_style_len = len(my_line_style)
                |""".stripMargin)

      print(s"""|Infinity = ${Double.MaxValue}
                |NaN = 0
                |""".stripMargin)
    }

    def pyChartQuote(label: String) = if (label.indexOf("/") >= 0) label.replace("/", "//") else label
  }
  object Plot {
    trait Style {
      def useColors = true
      def pyFillStyle: String
      def pyLineStyle: String
    }
    object Style {
      case object Colors1 extends Style {
        def pyLineStyle = """[line_style.T(width = 1.5, color = my_fill_style[0 % my_fill_style_len].bgcolor, dash = None),
                |                 line_style.T(width = 1.5, color = my_fill_style[1 % my_fill_style_len].bgcolor, dash = (3,4)),
                |                 line_style.T(width = 1.5, color = my_fill_style[2 % my_fill_style_len].bgcolor, dash = (6,5)),
                |                 line_style.T(width = 1.5, color = my_fill_style[3 % my_fill_style_len].bgcolor, dash = (8,4)),
                |                 line_style.T(width = 1.5, color = my_fill_style[4 % my_fill_style_len].bgcolor, dash = None),
                |                 line_style.T(width = 1.5, color = my_fill_style[5 % my_fill_style_len].bgcolor, dash = (1.5,1.5)),
                |                 line_style.T(width = 1.5, color = my_fill_style[6 % my_fill_style_len].bgcolor, dash = (5,2)),
                |                 line_style.T(width = 1.5, color = my_fill_style[7 % my_fill_style_len].bgcolor, dash = (3,3)),
                |                 line_style.T(width = 1.5, color = my_fill_style[8 % my_fill_style_len].bgcolor, dash = None),
                |                 line_style.T(width = 1.5, color = my_fill_style[9 % my_fill_style_len].bgcolor, dash = (1.5,1.5)),
                |                 ]""".stripMargin
        def pyFillStyle = """[rgb(238, 236, 184),
                |                 rgb(177, 204, 131),
                |                 rgb(163, 186, 196),
                |                 rgb(204, 175, 131),
                |                 rgb(231, 151, 151),
                |                 rgb(216, 233, 151),
                |                 rgb(192, 216,  96),
                |                 rgb(240, 128, 114),
                |                 rgb(241, 204, 216),
                |                 rgb(214,  77, 121),
                |                 ]""".stripMargin
      }
      case object Colors1NoDashes extends Style {
        def pyLineStyle = """[line_style.T(width = 1.5, color = my_fill_style[0 % my_fill_style_len].bgcolor, dash = None),
                |                 line_style.T(width = 1.5, color = my_fill_style[1 % my_fill_style_len].bgcolor, dash = None),
                |                 line_style.T(width = 1.5, color = my_fill_style[2 % my_fill_style_len].bgcolor, dash = None),
                |                 line_style.T(width = 1.5, color = my_fill_style[3 % my_fill_style_len].bgcolor, dash = None),
                |                 line_style.T(width = 1.5, color = my_fill_style[4 % my_fill_style_len].bgcolor, dash = None),
                |                 line_style.T(width = 1.5, color = my_fill_style[5 % my_fill_style_len].bgcolor, dash = None),
                |                 line_style.T(width = 1.5, color = my_fill_style[6 % my_fill_style_len].bgcolor, dash = None),
                |                 line_style.T(width = 1.5, color = my_fill_style[7 % my_fill_style_len].bgcolor, dash = None),
                |                 line_style.T(width = 1.5, color = my_fill_style[8 % my_fill_style_len].bgcolor, dash = None),
                |                 line_style.T(width = 1.5, color = my_fill_style[9 % my_fill_style_len].bgcolor, dash = None),
                |                 ]""".stripMargin
        def pyFillStyle = Colors1.pyFillStyle
      }
      case object Colors2 extends Style {
        def pyLineStyle = Colors1NoDashes.pyLineStyle
        def pyFillStyle = """[fill_style.Plain(bgcolor=color.lightblue),
                |                   fill_style.Plain(bgcolor=color.salmon),
                |                   fill_style.Plain(bgcolor=color.yellow2),
                |                   fill_style.Plain(bgcolor=color.darkseagreen2),
                |                   fill_style.Plain(bgcolor=color.plum),
                |                   fill_style.Plain(bgcolor=color.tan),
                |                   fill_style.gray90]"""
      }
      case object Colors3 extends Style {
        def pyLineStyle = Colors1NoDashes.pyLineStyle
        def pyFillStyle = """[rgb(156, 230, 228),
                |                 rgb(149, 207, 158),
                |                 rgb(172, 186, 115),
                |                 rgb(212, 138,  85),
                |                 rgb(214, 108,  79),
                |                 rgb(163, 219, 201),
                |                 rgb(179, 179, 142),
                |                 rgb(211, 145,  99),
                |                 rgb(226, 106,  96),
                |                 rgb(193,  96,  99),
                |                 ]"""
      }
      case object Colors4 extends Style {
        def pyLineStyle = Colors1NoDashes.pyLineStyle
        def pyFillStyle = """[rgb(136, 204, 136),
                |                 rgb(85, 170, 85),
                |                 rgb(45, 136, 45),
                |                 rgb(255, 170, 170),
                |                 rgb(212, 106, 106),
                |                 rgb(170, 57, 57),
                |                 ]"""
      }
      case object ColorsDivergingSpectral11 extends Style {
        def pyLineStyle = Colors1NoDashes.pyLineStyle
        def pyFillStyle = "[rgb(94,79,162),rgb(50,136,189),rgb(102,194,165),rgb(171,221,164),rgb(230,245,152),rgb(255,255,191),rgb(254,224,139),rgb(253,174,97),rgb(244,109,67),rgb(213,62,79),rgb(158,1,66)]"
      }
      case object ColorsDivergingSpectral9 extends Style {
        def pyLineStyle = Colors1NoDashes.pyLineStyle
        def pyFillStyle = "[rgb(50,136,189),rgb(102,194,165),rgb(171,221,164),rgb(230,245,152),rgb(255,255,191),rgb(254,224,139),rgb(253,174,97),rgb(244,109,67),rgb(213,62,79)]"
      }
      case object ColorsDivergingRdYlBu9 extends Style {
        def pyLineStyle = Colors1NoDashes.pyLineStyle
        def pyFillStyle = "[rgb(69,117,180),rgb(116,173,209),rgb(171,217,233),rgb(224,243,248),rgb(255,255,191),rgb(254,224,144),rgb(253,174,97),rgb(244,109,67),rgb(215,48,39)]"
      }
      case object ColorsDivergingRdYlBu11 extends Style {
        def pyLineStyle = Colors1NoDashes.pyLineStyle
        def pyFillStyle = "[rgb(49,54,149),rgb(69,117,180),rgb(116,173,209),rgb(171,217,233),rgb(224,243,248),rgb(255,255,191),rgb(254,224,144),rgb(253,174,97),rgb(244,109,67),rgb(215,48,39),rgb(165,0,38)]"
      }
      case object BW1 extends Style {
        override def useColors = false
        def pyLineStyle = Colors1.pyLineStyle
        def pyFillStyle = """[fill_style.Plain(bgcolor=color.gray10),
                |                 fill_style.Plain(bgcolor=color.gray40),
                |                 fill_style.Plain(bgcolor=color.gray70),
                |                 fill_style.Plain(bgcolor=color.gray90),
                |                 ]"""
      }
      case object BW2 extends Style {
        override def useColors = false
        def pyLineStyle = Colors1.pyLineStyle
        def pyFillStyle = """[fill_style.gray50, fill_style.gray90, fill_style.diag, fill_style.white, fill_style.gray20, fill_style.rdiag, fill_style.vert, fill_style.gray30, fill_style.gray10]"""
      }
    }
  }

  abstract class BarPlotCommon extends Plot {
    var barWidth: Double = 10.0

    override def autoWidth = {
      val nbars = series.size * xCoordsArray.size
      ((nbars * barWidth) / 2 + series.size * barWidth / 4).round.toInt
    }

    var xAxisLabelStairs: Int = 1
    var xAxisLabelStairInc: Double = fontSize * 1.1

    var outOfRangeLabelScale: Double = 0.6
    var outOfRangeLabelXoffset: Double = barWidth / 2
    var outOfRangeLabelYoffset: Double = 0.0
    var outOfRangeLabelYoffsetInc: Double = (fontSize * outOfRangeLabelScale) + 1.0

    protected def printPyChartImprovementCode(): Unit = {
      print(s"""|class XL(pychart.axis.X):
                |    def draw_tics_and_labels(self, ar, can):
                |        assert self.check_integrity()
                |        y_base = ar.loc[1] + self.offset
                |        self.tic_interval = self.tic_interval or ar.x_grid_interval
                |
                |        can.line(self.line_style, ar.loc[0], y_base,
                |             ar.loc[0]+ ar.size[0], y_base)
                |
                |        tic_dic = {}
                |        max_tic_height = 0
                |        if self.draw_tics_above:
                |            sign = 1
                |        else:
                |            sign = -1
                |
                |        tic_index = 0
                |        for i in ar.x_tic_points(self.tic_interval):
                |            tic_dic[i] = 1
                |            ticx = ar.x_pos(i)
                |
                |            string = "/hC" + pychart_util.apply_format(self.format, (i, ), 0)
                |
                |            (total_height, base_height) = font.text_height(string)
                |            max_tic_height = max(max_tic_height, total_height)
                |            if self.draw_tics_above:
                |                base_height = 0
                |
                |            can.line(self.line_style, ticx, y_base,
                |                 ticx, y_base + sign * self.tic_len)
                |            can.show(ticx + self.tic_label_offset[0],
                |                 y_base + sign * (self.tic_len + base_height + (tic_index % $xAxisLabelStairs) * $xAxisLabelStairInc) + self.tic_label_offset[1],
                |                 string)
                |            tic_index = tic_index + 1
                |
                |        if self.minor_tic_interval:
                |            for i in ar.x_tic_points(self.minor_tic_interval):
                |                if tic_dic.has_key(i):
                |                    # a major tic was drawn already.
                |                    pass
                |                else:
                |                    ticx = ar.x_pos(i)
                |                    can.line(self.line_style, ticx, y_base,
                |                             ticx, y_base + sign * self.minor_tic_len)
                |
                |        self.draw_label(ar, can, (y_base + sign *
                |                                  (self.tic_len + max_tic_height + 10)))
                |""".stripMargin)

      print("""|class nodupslegend(pychart.legend.T):
               |    def draw(self, ar, entries, can):
               |        def reorder(l, numRows):
               |            size = len(l)
               |            fullRows = size % numRows
               |            def rowStart(r):
               |                if r > fullRows:
               |                    return (size / numRows + 1) * fullRows + (size / numRows) * (r - fullRows)
               |                else:
               |                    return (size / numRows + 1) * r
               |            ret = ['undefined'] * size
               |            for i in range(0,size):
               |                row = i % numRows
               |                col = i / numRows
               |                ret[i] = l[rowStart(row) + col]
               |            return ret
               |        entries_without_dups = []
               |        for i in entries:
               |            curr = [e.label for e in entries_without_dups]
               |            if i.label not in curr:
               |                entries_without_dups.append(i)
               |        entries = reorder(entries_without_dups, self.nr_rows)
               |        pychart.legend.T.draw(self, ar, entries, can)
               |""".stripMargin)
    }

    override def xCoordsArray = getXValues.filterDuplicates.map(_.toString)
    protected def printXcoordsArray(): Unit = {
      print("x_coords = [")
      xCoordsArray foreach { v => print(f"  [unicode('${pyChartQuote(v)}')],\n") }
      print("]\n")
    }

    protected def printSeparationLines(): Unit =
      separationLines foreach { xCoordToTheLeft =>
        println(s"line_x = (ar.x_pos(x_coords[${xCoordToTheLeft}][0]) + ar.x_pos(x_coords[${xCoordToTheLeft - 1}][0])) / 2")
        println(s"canvas.line(line_style.black_dash1, line_x, ar.y_pos(ar.y_range[0]), line_x, ar.y_pos(ar.y_range[1]))")
      }
  }

  class BarPlot extends BarPlotCommon {
    def printCode() = {
      printPyChartCommonCode()
      printPyChartImprovementCode()

      printXcoordsArray()

      val seriesLength = series.size
      if (seriesLength > 0) {
        print(s"""|ar = area.T(size = area_size,
                  |            x_coord = category_coord.T(x_coords, 0),
                  |            y_range = ($yRangeMin, $yRangeMax),
                  |            y_grid_interval = $yGridInterval,
                  |            x_axis = XL(label = unicode('${pyChartQuote(xAxisTitle)}', 'utf-8'), format=${if (xAxisHide) "lambda x: ''" else s"'$xAxisLabelFormat'"}),
                  |            y_axis = axis.Y(label = unicode('${pyChartQuote(yAxisTitle)}', 'utf-8'), format='$yAxisLabelFormat'),
                  |            legend = ${if (seriesLegend) s"nodupslegend(nr_rows = $seriesLegendRows, loc = legend_loc)" else "None"})
                  |""".stripMargin)

        println(s"# $seriesLength series")
        for ((s, idx) <- series.zipWithIndex) {
          println(f"# $idx%d '${s.label}%s'")
          println("serie_data = [")
          def p(x: String, y: CoordValue) =
            if (y.noCoordValue == None || y.value.isInfinity || y.value.isNaN) println(s" [unicode('${pyChartQuote(x)}'), 0, 0],")
            else println(s" [unicode('${pyChartQuote(x)}'), ${y.value}, ${y.error}],")

          for (Point(x, y) <- s.data) p(x.toString, y)
          print("]\n\n")

          println(s"""|ar.add_plot(bar_plot.T(data = serie_data,
                      |            cluster = ($idx, $seriesLength),
                      |            label = unicode('${pyChartQuote(s.label)}'),
                      |            error_bar = error_bar.bar2, error_minus_col = 2,""".stripMargin)
          println(s"            fill_style = my_fill_style[$idx % my_fill_style_len],")
          println(s"            width = $barWidth))")
        }
        println("ar.draw()")
        printSeparationLines()
        val yOffsets = collection.mutable.HashMap[Any, Double]()
        for (s <- series; Point(x, y) <- s.data; if y != None && y.value > yRangeMax) {
          val yOffset = yOffsets.getOrElse(x, outOfRangeLabelYoffset)
          yOffsets(x) = yOffset - outOfRangeLabelYoffsetInc
        }
        val outOfRangeLabelFontSize = (outOfRangeLabelScale * fontSize).round
        for ((s, idx) <- series.zipWithIndex; Point(x, y) <- s.data; if y != None && y.value > yRangeMax) {
          val xOffset = outOfRangeLabelXoffset + barWidth * seriesLength / 2
          val arrowHeadLen = barWidth * 0.7
          val xTargetOffset = idx * barWidth - barWidth * seriesLength / 2 + barWidth / 2 - arrowHeadLen / 2
          val yOffset = yOffsets(x)
          val arrowYOffset = yOffset + outOfRangeLabelFontSize / 3
          if (y.value.isInfinity) printf("canvas.show(ar.x_pos(unicode('%s')) + %s, ar.y_pos(%s) + %s, \"/%d{}Inf\")\n", pyChartQuote(x.toString), xOffset, yRangeMax, yOffset, outOfRangeLabelFontSize)
          else printf("canvas.show(ar.x_pos(unicode('%s')) + %s, ar.y_pos(%s) + %s, \"/%d{}%s\" %% %s)\n", pyChartQuote(x.toString), xOffset, yRangeMax, yOffset, outOfRangeLabelFontSize, yAxisLabelFormat, y.value)
          printf("a = arrow.T(head_style = 2, thickness = 1, head_len = %s)\n", arrowHeadLen)
          printf("a.draw([(ar.x_pos(unicode('%s')) + %s, ar.y_pos(%s) + %s), (ar.x_pos(unicode('%s')) + %s, ar.y_pos(%s) + %s)])\n", pyChartQuote(x.toString), xOffset, yRangeMax, arrowYOffset, pyChartQuote(x.toString), xTargetOffset, yRangeMax, arrowYOffset)
          yOffsets(x) = yOffset + outOfRangeLabelYoffsetInc
        }

      } else {
        print(s"""|tb = text_box.T(loc = (0,0), text="/hCEmpty")
                  |tb.draw()
                  |""".stripMargin)
      }
    }

    def printTsv() = printTsvSeriesAsColumns()

    def printTsvSeriesAsRows(): Unit = {
      def fmt(x: Any) = x.toString
      val columns = getXValues.filterDuplicates
      println("\t" + columns.map(col => fmt(col) + "\t" + fmt(col) + "_err").mkString("\t"))
      series foreach { s =>
        val values = s.data.map(p => (p.x, p.y)).toMap
        println(s.label + "\t" +
                columns.map(x => values.get(x).map(v => fmt(v.value) + "\t" + fmt(v.error)).getOrElse("\t")).mkString("\t"))
      }
    }

    def printTsvSeriesAsColumns(): Unit = {
      def fmt(x: Any) = x.toString
      val rows = getXValues.filterDuplicates
      println("\t" + series.map(col => fmt(col.label)).mkString("\t"))
      val values = series.map(s => s -> s.data.map(p => (p.x, p.y)).toMap).toMap
      rows foreach { r =>
        print(fmt(r))
        series.foreach { s =>
          print("\t")
          print(values(s).get(r).map(y => fmt(y.value)).getOrElse(""))
        }
        println()
        print(fmt(r) + "_err")
        series.foreach { s =>
          print("\t")
          print(values(s).get(r).map(y => fmt(y.error)).getOrElse(""))
        }
        println()
      }
    }
  }

  class StackedBarPlot extends BarPlotCommon {
    override def getYValues: Iterable[Iterable[(Any, Any)]] = try {
      series flatMap (_.data) map {
        case Point(_, v: Iterable[_]) => v.zipWithIndex map {
          case (x@(_, _), _) => x
          case (x, i) => i.toString -> x
        }
        case Point(_, v: (_, _)) => Seq(v)
        case Point(_, v) => Seq("all" -> v)
      }
      //series flatMap (_.data) map (_.y.asInstanceOf[Seq[(Any, Any)]])
    } catch {
      case e: Throwable =>
        Console.withOut(Console.err) {
          println(s"series: «$series»")
          series flatMap (_.data) foreach { i => println(s"«$i» ${i.x.getClass} ${i.y.getClass}") }
        }
        throw e
    }
    override def getValidTotalYValues = getYValues.map { l => (l collect { case (_, x) if !x.isInfinity && !x.isNaN => x.value }).foldLeft(0.0)(_ + _) }

    var categoriesLegend = true
    var categoriesLegendRows = 1

    var missingCategoryValue = 0.0
    var categoriesOrder: Option[(Any, Any) => Boolean] = None
    def getCategories = {
      val l = (getYValues flatMap { _ map (_._1) }).filterDuplicates
      categoriesOrder match {
        case Some(f) => l sortWith f
        case None    => l
      }
    }

    def categoryName(c: Any) = c match {
      case s: Symbol => s.name
      case _         => c.toString
    }

    def printCode() = {
      printPyChartCommonCode()
      printPyChartImprovementCode()

      printXcoordsArray()

      if (series.nonEmpty && getCategories.nonEmpty) {
        println(s"ar = area.T(size = area_size,")
        printf("    x_coord = category_coord.T(x_coords, 0),\n")
        printf("    y_range = (%s, %s),\n", yRangeMin, yRangeMax)
        printf("    y_grid_interval = %s,\n", yGridInterval)
        println(s"    x_axis = XL(label = unicode('${pyChartQuote(xAxisTitle)}', 'utf-8'), format=${if (xAxisHide) "lambda x: ''" else s"'$xAxisLabelFormat'"}),")
        printf("    y_axis = axis.Y(label = unicode('%s', 'utf-8'), format='%s'),\n", pyChartQuote(yAxisTitle), yAxisLabelFormat)
        if (categoriesLegend) println(s"    legend = nodupslegend(nr_rows = $categoriesLegendRows, loc = legend_loc))")
        else printf("    legend = None)\n")

        printf("# %d series\n", series.size)
        for ((s, idx) <- series.zipWithIndex) {
          printf("# %d '%s'\n", idx, s.label)
          println("prev_plot = None")
          println("bar_plot.fill_styles.reset()\n")
          for ((c, catIdx) <- getCategories.zipWithIndex) {
            print("serie_data = [\n")
            def p(x: String, v: Double, e: Double) =
              if (v.isInfinity || v.isNaN) println(s" [unicode('${pyChartQuote(x)}'), 0, 0],")
              else println(s" [unicode('${pyChartQuote(x)}'), $v, $e],")
            for (Point(x, yl) <- s.data) {
              val px = yl.asInstanceOf[Iterable[(Any, Any)]].toMap.getOrElse(c, missingCategoryValue)
              p(x.toString, px.value, px.error)
            }
            print("]\n")
            printf("p = bar_plot.T(data = serie_data,\n")
            printf("    cluster = (%s, %s),\n", idx, series.size)
            printf("    label = '%s',\n", pyChartQuote(categoryName(c)))
            printf("    error_bar = error_bar.bar2, error_minus_col = 2,")
            printf("    width = %s,\n", barWidth)
            printf("    fill_style = my_fill_style[%s %% my_fill_style_len],\n", catIdx)
            printf("    stack_on = prev_plot)\n")
            printf("ar.add_plot(p)\n")
            printf("prev_plot = p\n")
          }
        }
        println("ar.draw()")
        printSeparationLines()

        def sumMap(a: Any) = {
          val m = a.asInstanceOf[Iterable[(Any, Any)]]
          (m map (_._2.value)).foldLeft(0.value)(_ + _)
        }
        val yOffsets = collection.mutable.HashMap[Any, Double]()
        for (s <- series; item <- s.data; if sumMap(item.y) > yRangeMax) {
          val yOffset = yOffsets.getOrElse(item.x, outOfRangeLabelYoffset)
          yOffsets(item.x) = yOffset - outOfRangeLabelYoffsetInc
        }
        val outOfRangeLabelFontSize = (outOfRangeLabelScale * fontSize).round
        for ((s, idx) <- series.zipWithIndex; item <- s.data; if sumMap(item.y) > yRangeMax) {
          val xOffset = outOfRangeLabelXoffset + barWidth * series.size / 2
          val arrowHeadLen = barWidth * 0.7
          val xTargetOffset = idx * barWidth - barWidth * series.size / 2 + barWidth / 2 - arrowHeadLen / 2
          val yOffset = yOffsets(item.x)
          val arrowYOffset = yOffset + outOfRangeLabelFontSize / 3

          if (sumMap(item.y).isInfinity) println(s"canvas.show(ar.x_pos(unicode('${pyChartQuote(item.x.toString)}')) + $xOffset, ar.y_pos($yRangeMax) + $yOffset, ${'"'}/$outOfRangeLabelFontSize{}Inf${'"'})")
          else println(s"canvas.show(ar.x_pos(unicode('${pyChartQuote(item.x.toString)}')) + $xOffset, ar.y_pos($yRangeMax) + $yOffset, ${'"'}/$outOfRangeLabelFontSize{}$yAxisLabelFormat${'"'} % ${sumMap(item.y)})")
          println(s"a = arrow.T(head_style = 2, thickness = 1, head_len = $arrowHeadLen)")
          println(s"a.draw([(ar.x_pos('${pyChartQuote(item.x.toString)}') + $xOffset, ar.y_pos($yRangeMax) + $arrowYOffset), (ar.x_pos(unicode('${pyChartQuote(item.x.toString)}')) + $xTargetOffset, ar.y_pos($yRangeMax) + $arrowYOffset)])")
          yOffsets(item.x) = yOffset + outOfRangeLabelYoffsetInc
        }

        if (seriesLegend) {
          val labels = series map (s => pyChartQuote(s.label))
          val elemsPerRow = math.ceil(labels.size.toFloat / seriesLegendRows).toInt
          val legendText = (0 until seriesLegendRows) map { r => labels.slice(r * elemsPerRow, (r + 1) * elemsPerRow).mkString("  ") } mkString "\\n"
          print(s"""|legend_series_loc = (mm_to_pt($legendOffsetX), area_size[1] + mm_to_pt($legendOffsetY) - ${fontSize * (seriesLegendRows + 0.5)})
                    |ls = text_box.T(loc = legend_series_loc, text="/hL$legendText")
                    |ls.draw()
                    |""".stripMargin)
        }
      } else {
        print(s"""|tb = text_box.T(loc = (0,0), text="/hCEmpty")
                  |tb.draw()
                  |""".stripMargin)
      }
    }

    def printTsv() = printTsvSeriesAsColumns()

    def printTsvSeriesAsRows(): Unit = {
      def fmt(x: Any) = x.toString
      val categories = getCategories
      val columns = getXValues.filterDuplicates
      println("\t" + columns.flatMap(col =>
        categories.map { c =>
          fmt(col) + "/" + fmt(c) + "\t" + fmt(col) + "/" + fmt(c) + "_err"
        }).mkString("\t"))
      series foreach { s =>
        val values = s.data.map(p => (p.x, p.y)).toMap
        println(s.label + "\t" +
                columns.flatMap { x =>
                  val y = values.getOrElse(x, Seq.empty[Iterable[(Any, Any)]]).asInstanceOf[Iterable[(Any, Any)]].toMap
                  categories.map { c =>
                    val v = y.getOrElse(c, missingCategoryValue)
                    fmt(v.value) + "\t" + fmt(v.error)
                  }
                }.mkString("\t"))
      }
    }

    def printTsvSeriesAsColumns(): Unit = {
      def fmt(x: Any) = x.toString
      val rows = getXValues.filterDuplicates
      println("\t" + series.map(col => fmt(col.label)).mkString("\t"))
      val values = series.map(s => s -> s.data.map(p => (p.x, p.y.asInstanceOf[Iterable[(Any, Any)]])).toMap).toMap
      rows foreach { r =>
        val categories = values.toSeq.flatMap(_._2.values.flatMap(_.map(_._1))).filterDuplicates
        categories foreach { c =>
          print(fmt(r) + "/" + c)
          series.foreach { s =>
            print("\t")
            print(values(s).get(r).map(y => fmt(y.toMap.get(c).map(_.value).getOrElse(""))).getOrElse(""))
          }
          println()
          print(fmt(r) + "/" + c + "_err")
          series.foreach { s =>
            print("\t")
            print(values(s).get(r).map(y => fmt(y.toMap.get(c).map(_.error).getOrElse(""))).getOrElse(""))
          }
          println()
        }
      }
    }
  }

  class LinePlot extends Plot {
    var _xRangeMin: Option[Double] = None
    def xRangeMin_=(v: Double): Unit = { _xRangeMin = Some(v) }
    def xRangeMin = _xRangeMin getOrElse { getXValuesMin - .05 * getXValuesRangeSize }

    var _xRangeMax: Option[Double] = None
    def xRangeMax_=(v: Double): Unit = { _xRangeMax = Some(v) }
    def xRangeMax = _xRangeMax getOrElse { getXValuesMax + .05 * getXValuesRangeSize }

    var useCategorizedXcoords = false
    var categorizedXcoordsTickInterval = 1

    var _xGridInterval: Option[Double] = None
    def xGridInterval_=(v: Double): Unit = { _xGridInterval = Some(v) }
    def xGridInterval = _xGridInterval getOrElse { (xRangeMax - xRangeMin) / 8.01 }

    def getXValuesMin: Double = (getXValues filter (x => !x.isInfinity && !x.isNaN)).foldLeft(Double.MaxValue: CoordValue)(_ min _).toDouble
    def getXValuesMax: Double = (getXValues filter (x => !x.isInfinity && !x.isNaN)).foldLeft(Double.MinValue: CoordValue)(_ max _).toDouble
    def getXValuesRangeSize = Math.abs(getXValuesMax - getXValuesMin)

    override def autoWidth = xCoordsArray.size * 10

    override def printCode(): Unit = {
      printPyChartCommonCode()
      if (useCategorizedXcoords) printXcoordsArray()

      printf("ar = area.T(size = area_size,\n")
      printf("    y_range = (%s, %s),\n", yRangeMin, yRangeMax)
      if (useCategorizedXcoords) printf("    x_range = (0, %s),\n", (series.map(_.data.size).maxOption).getOrElse(0) + 1)
      else printf("    x_range = (%s, %s),\n", xRangeMin, xRangeMax)
      printf("    y_grid_interval = %s,\n", yGridInterval)
      if (!useCategorizedXcoords) printf("    x_grid_interval = %s,\n", xGridInterval)
      if (useCategorizedXcoords && !xAxisHide) printf(s"    x_axis = axis.X(label = '%s', format=lambda x: x_coord_fmt(x), tic_interval=$categorizedXcoordsTickInterval),\n", xAxisTitle)
      else println(s"    x_axis = axis.X(label = unicode('${pyChartQuote(xAxisTitle)}'), format=${if (xAxisHide) "lambda x: ''" else s"'$xAxisLabelFormat'"}),")
      printf("    y_axis = axis.Y(label = unicode('%s'), format='%s'),\n", pyChartQuote(yAxisTitle), yAxisLabelFormat)
      if (seriesLegend) printf("    legend = legend.T(nr_rows = %s, loc = legend_loc))\n", seriesLegendRows)
      else printf("    legend = None)\n")

      printf("# %d series\n", series.size)
      for ((s, idx) <- series.zipWithIndex) {
        printf("# '%s'\n", s.label)
        print("serie_data = [\n")
        for ((Point(x, y), xindex) <- s.data.zipWithIndex) {
          if (useCategorizedXcoords) printf("  [%s, %s, %s],\n", xindex + 1, y.value, y.error)
          else printf("  [%s, %s, %s],\n", x, y.value, y.error)
        }
        print("]\n\n")
        printf("ar.add_plot(line_plot.T(data = serie_data,\n")
        printf("    label = '%s',\n", pyChartQuote(s.label))
        printf("    line_style = my_line_style[%s %% my_line_style_len],\n", idx)
        printf("    error_bar = error_bar.bar2, y_error_minus_col = 2))\n")
      }
      println("ar.draw()")
    }

    protected def printXcoordsArray(): Unit = {
      println("x_coords = [")
      getXValues.filterDuplicates foreach { v => println(f"  unicode('${pyChartQuote(v.toString)}'),") }
      println("]")
      println(
        s"""|def x_coord_fmt(x):
            |    if x > 0 and x <= len(x_coords):
            |        return "$xAxisLabelFormat" % x_coords[int(x-1)]
            |    else:
            |        return ""
            |""".stripMargin)
    }

    def printTsv(): Unit = {
      def fmt(x: Any) = x.toString
      val columns = getXValues.filterDuplicates
      println("\t" + columns.map(col => fmt(col) + "\t" + fmt(col) + "_err").mkString("\t"))
      series foreach { s =>
        val values = s.data.map(p => (p.x, p.y)).toMap
        println(s.label + "\t" +
                columns.map(x => values.get(x).map(v => fmt(v.value) + "\t" + fmt(v.error)).getOrElse("\t")).mkString("\t"))
      }
    }
  }
}
