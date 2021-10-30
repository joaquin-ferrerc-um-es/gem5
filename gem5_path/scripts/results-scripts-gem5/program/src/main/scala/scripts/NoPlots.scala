package scripts

import repscr._
import properties._
import util.misc._
import collection.immutable.TreeMap

object NoPlots extends App with PlotScript {
  // just parse the input dir and show summary of configs

  val unchanging = findConstantConfigs(mixes map (_.configs))
  println("Unchanging config parameters:")
  unchanging foreach {
    case (k, v) => println(s"  ${k}: ${v}")
  }

  def findConstantConfigs(l: Iterable[PropertyMap]) = {
    val keys = (l map (_.keySet)).reduceOption(_ ++ _).getOrElse(Set())
    val r = (for (k <- keys.toSeq)
      yield k ->
        (for (c <- l)
          yield c get k match {
            case Some(v) => v
            case None => None
          })).filter{
      _._2.allEquals
    }.map{ case (k, l) => k -> l.head }
    TreeMap(r: _*)
  }
}
