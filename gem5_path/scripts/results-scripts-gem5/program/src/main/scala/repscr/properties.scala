package repscr

import scala.language.implicitConversions

import collection.immutable.SortedMap
import collection.immutable.TreeMap
import util.misc._

object properties {
  type PropertyMap = SortedMap[String, Any]
  object PropertyMap {
    def apply(): PropertyMap = TreeMap.empty
    def apply(t: SortedMap[String, Any]): PropertyMap = t
    def apply(t: IterableOnce[(String, Any)]): PropertyMap = this () ++ t

    type PropertyVariations = SortedMap[String, Iterable[Any]]
    def findVariations(l: Iterable[PropertyMap]): PropertyVariations = {
      val keys = (l.map(_.keySet.unsorted)).reduce(_ ++ _)
      TreeMap.empty[String, Iterable[Any]] ++ keys.view.map(k =>
        k -> l.map(c =>
          c get k match {
            case Some(v) => v
            case None => None
          })).filter(!_._2.allEquals)
    }
    def findConstants(l: Iterable[PropertyMap]): PropertyMap = {
      val keys = (l map (_.keySet.unsorted)).reduce(_ ++ _)
      TreeMap.empty[String, Any] ++ keys.view.map(k =>
        k -> l.map(c =>
          c get k match {
            case Some(v) => v
            case None => None
          })).filter(_._2.allEquals).map { case (k, l) => k -> l.head }
    }
  }
}
