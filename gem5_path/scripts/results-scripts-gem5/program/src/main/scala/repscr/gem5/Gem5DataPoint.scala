package repscr.gem5

import repscr._
import properties._
import points._
import util.misc._

trait Gem5DataPoint extends DataPoint {
  def configs = properties.filter { case (k, v) => Gem5Properties.knownProperties(k).kind == Gem5Properties.Config }
  def results = properties.filter { case (k, v) => Gem5Properties.knownProperties(k).kind == Gem5Properties.Result }
  def files: Iterable[String] // all files that contributed to this datapoint
  override def toString = files.sortAsStrings.mkString("'", "',\n'", "': ") +
                          configs.toSeq.sortBy(_._1).map { case (k, v) => s"$k: $v" }.mkString("{\n  ", "\n  ", " } -> ") +
                          results.toSeq.sortBy(_._1).map { case (k, v) => s"$k: $v" }.mkString("{\n  ", "\n  ", " }")
  // Better accessors for some properties:
  def num_cpus = this("num_cpus").value.toInt
  def protocol = this("protocol").toString
  def benchmarkName = this("benchmark_name").toString
  def benchmarkSize = this("benchmark_size").toString
}
object Gem5DataPoint {
  def findConfigVariations(l: Iterable[Gem5DataPoint]) = PropertyMap.findVariations(l map { _.configs })
  def findConstantConfigs(l: Iterable[Gem5DataPoint]) = PropertyMap.findConstants(l map { _.configs })
}
