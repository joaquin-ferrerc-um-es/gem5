package repscr

import language.implicitConversions
import util.misc.dynamicOrdering

trait PlotCoordinates[T] { self =>
  type DataPointType = T

  case class Coord(
    name: String,
    fn: DataPointType => Any,
    isConfig: Boolean = false,
    stacked: Boolean = false,
    axisTitle: String = "????",
    ordering: Ordering[Any] = dynamicOrdering(),
    doc: String = "????",
    private val transient: Boolean = false) {
    def map(fn: Any => Any, name: String = name, axisTitle: String = axisTitle, ordering: Ordering[Any] = ordering, stacked: Boolean = stacked) = copy(fn = { x => fn(this.fn(x)) }, name = name, axisTitle = axisTitle, ordering = ordering, stacked = stacked, transient = true)
    def derivedCoord(newName: String, fnMap: Any => Any, axisTitle: String = axisTitle, ordering: Ordering[Any] = ordering, stacked: Boolean = stacked) = map(fnMap, axisTitle = axisTitle, ordering = ordering, stacked = stacked).copy(name = newName, transient = false)
    override def toString = name
    def help = if (doc == "????") axisTitle else doc
    def optFn(s: T) = try Some(fn(s)) catch {
      case e: NoSuchElementException =>
          warning(s"Coord ${this} not found in ${s}")
        None
    }
    if (!transient) addCoord(this)
  }

  def warning(msg: String): Unit = Console.err.println(s"Warning: $msg")

  private var coordsMap = Map.empty[String, Coord]
  def addCoord(c: Coord): Unit = coordsMap += (c.name -> c)
  def allCoords: Map[String,Coord] = coordsMap

  def defaultStringToCoord(s: String): Coord = throw new IllegalArgumentException()

  def toCoord(s: String): Coord = coordsMap.getOrElse(s, defaultStringToCoord(s))
  implicit class stringToCoord(s: String) {
    def toCoord: Coord = self.toCoord(s)
  }

  implicit val coordOrdering: Ordering[Coord] = Ordering.fromLessThan { (a, b) => a.name < b.name }
}
