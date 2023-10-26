package util

import java.util.NoSuchElementException

import collection.mutable.{Map => MMap}
import scala.util.matching.Regex
import scala.language.{postfixOps, implicitConversions}

object RMap {
  type Key = String
  type Value = String
  type Children = collection.Map[Key, Node]
  type MutableChildren = collection.mutable.Map[Key, Node]
  type Path = Seq[Key]
  def parent(path: Path): Path = path.dropRight(1)

  sealed trait Node {
    def children: Children
    private[RMap] def mutable_children: MutableChildren
    def value: Option[Value]

    def go(path: Key*): Node = path.foldLeft(this) {
      case (NodeBoth(v, c), k)  => c(k)
      case (NodeChildren(c), k) => c(k)
      case (NodeValue(_), k)    => throw new NoSuchElementException(s"key not found: $k")
    }
    def get(): Option[Value] = value
    def get(k: Key): Option[Value] = children.get(k) match {
      case Some(NodeBoth(v, _))  => Some(v)
      case Some(NodeValue(v))    => Some(v)
      case Some(NodeChildren(_)) => None
      case None                  => None
    }
    def get(path: Key*): Option[Value] = {
      if (path.isEmpty) value
      else go(parent(path): _*).children.get(path.last) match {
        case Some(NodeBoth(v, _))  => Some(v)
        case Some(NodeValue(v))    => Some(v)
        case Some(NodeChildren(_)) => None
        case None                  => None
      }
    }
    def apply(path: Key*): Value = get(path: _*) match {
      case Some(v) => v
      case None    => throw new NoSuchElementException(s"path not found ${path.mkString(".")}")
    }

    def /(ks: Key): Node = children(ks)
    def /?(ks: Key): Option[Node] = children.get(ks)
    def /(ki: Regex): Iterable[Node] = children.collect { case (i@ki(_*), n) => n }
    def /-(ki: Regex): Iterable[(String, Node)] = children.collect { case (i@ki(x), n) => x -> n }
    def /--(ki: Regex): Iterable[(String, String, Node)] = children.collect { case (i@ki(x, y), n) => (x, y, n) }
    def /+(ks: Key): Option[Value] = get(ks)
    def /+(ks: Regex): Iterable[Value] = /(ks) flatMap (_.value)
    def /+-(ks: Regex): Iterable[(String, Value)] = /-(ks) flatMap { case (k, n) => n.value.map(k -> _) }

    def flatten: Iterable[(Path, Value)] = {
      def iter(parentPath: Path, n: Node): Iterable[(Path, Value)] = {
        val x = n.value.map(parentPath -> _)
        val c = n.children.toSeq.flatMap { case (key, node) => iter(parentPath :+ key, node) }
        x ++ c
      }
      iter(Seq(), this)
    }

    override def toString = flatten.map {case (l,v) =>  s"${l.mkString(".")} → $v"}.mkString("\n")
  }

  case class NodeValue(v: Value) extends Node {
    def value = Some(v)
    def children: Children = Map.empty
    def mutable_children = throw new NoSuchElementException()
  }
  case class NodeChildren(
    children: MutableChildren = MMap.empty) extends Node {
    def value = None
    def mutable_children = children
  }
  case class NodeBoth(v: Value,
    children: MutableChildren = MMap.empty) extends Node {
    def value = Some(v)
    def mutable_children = children
  }

  implicit def rmap2Node(m: RMap): Node = m.root

  implicit class seqNode(s: Iterable[Node]) {
    def /(ks: Key): Iterable[Node] = s.map(_./(ks))
    def /?(ks: Key): Iterable[Node] = s.flatMap(_./?(ks))
    def /(ks: Regex): Iterable[Node] = s.flatMap(_./(ks))
    def /-(ks: Regex): Iterable[(String, Node)] = s.flatMap(_./-(ks))
    def /--(ks: Regex): Iterable[(String, String, Node)] = s.flatMap(_./--(ks))
    def /+(ks: Key): Iterable[Value] = s.flatMap(_.get(ks))
    def /+(ks: Regex): Iterable[Value] = s.flatMap(_./(ks)) flatMap (_.value)
    def /+-(ks: Regex): Iterable[(String, Value)] = s.flatMap(_./-(ks)) flatMap { case (k, n) => n.get().map(k -> _) }
    def /+--(ks: Regex): Iterable[(String, String, Value)] = s.flatMap(_./--(ks)) flatMap { case (k1, k2, n) => n.get().map((k1, k2, _)) }
  }
}
class RMap(val root: RMap.Node = RMap.NodeChildren()) {
  import RMap._

  override def toString = "+" + root

  def set(path: Key*)(v: Value): RMap = {
    var parentNode = root
    parent(path).foreach { k =>
      //assert(parentNode match { case NodeValue(_) => false case NodeChildren(_) => true case NodeBoth(_, _) => true })
      val child = parentNode.children.get(k) match {
        case Some(n@NodeChildren(_)) => n
        case Some(n@NodeBoth(_, _))  => n
        case Some(NodeValue(pv))     =>
          val n = NodeBoth(pv)
          parentNode.mutable_children(k) = n
          n
        case None                    =>
          val n = NodeChildren()
          parentNode.mutable_children(k) = n
          n
      }
      parentNode = child
    }
    val lp = path.last
    val n = parentNode.children.get(lp) match {
      case Some(NodeChildren(c))     => NodeBoth(v, c)
      case Some(NodeBoth(_, c))      => NodeBoth(v, c)
      case None | Some(NodeValue(_)) => NodeValue(v)
    }
    parentNode.mutable_children(lp) = n
    this
  }
}
object TestRMap extends App {
  val rm = new RMap()

  rm.set("a")("xxx1")
  rm.set("b")("xxx2")
  rm.set("c")("xxx3")
  rm.set("a", "b")("xxx33")
  rm.set("a", "b", "c")("xxx34")
  rm.set("a", "b")("xxx2000")
  rm.set("a", "b", "j", "k", "l")("xxx700")
  rm.set("a", "b", "w", "k", "l")("xxx701")
  rm.set("a", "b", "w", "t", "a")("xxx721")
  rm.set("a", "b", "w", "g", "x")("xxx731")
  rm.set("a", "b", "j", "t", "l")("xxx709")
  rm.set("a", "b", "x", "k", "l")("xxx702")
  rm.set("c")("xxx4")

  println(rm.get("a"))
  /*println(rm.go("a"))
  println(rm.go("c"))
  println(rm.go("a", "b"))
  println(rm.go("a", "b", "j", "k"))
  println(rm.go("a", "b", "j", "k", "l"))*/

  println(rm / "a" / "b" / "[jw]".r / "[kt]".r / ".".r)

  println(rm / "a")
  println(rm /+ "a")

  println(rm.flatten.mkString("\n"))
}