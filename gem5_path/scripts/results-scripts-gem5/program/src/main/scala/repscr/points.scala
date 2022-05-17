package repscr

import scala.language.implicitConversions
import properties._

import scala.annotation.tailrec

object points {
  /* Dynamic typing for coordinate values */
  implicit class CoordValue(val any: Any) extends AnyVal {
    @tailrec
    private def o: Any = any match {
      case c: CoordValue => c.o
      case _             => any
    }
    def noCoordValue = o
    private def oType = o.getClass

    override def toString = o.toString

    def toType[A](implicit m: Manifest[A]) = o match {
      case x: A => x
      case _    => sys.error(s"Not of type ${m}, but  $oType ($o)")
    }

    def toDouble: Double = o match {
      case x: Double => x
      case x: Long   => x.toDouble
      case x: Int    => x.toDouble
      case Some(x)   => CoordValue(x).toDouble
      case _         => sys.error(s"Not a number ($o) : $oType")
    }
    def toInt: Int = o match {
      case x: Int  => x
      case Some(x) => CoordValue(x).toInt
      case _       => toDouble.toInt
    }
    def toLong: Long = o match {
      case x: Long => x
      case x: Int  => x
      case Some(x) => CoordValue(x).toLong
      case _       => toDouble.toLong
    }
    def toBoolean: Boolean = o match {
      case x: Boolean => x
      case Some(x)    => CoordValue(x).toBoolean
      case _          => sys.error(s"Not a boolean ($o) : $oType")
    }
    def value: Double = o match {
      case x: Double => x
      case x: Vwe    => x.value
      case x: Long   => x.toDouble
      case x: Int    => x.toDouble
      case Some(x)   => CoordValue(x).value
      case _         => sys.error(s"Not a number ($o) : $oType")
    }
    def error: Double = o match {
      case x: Double => 0
      case x: Vwe    => x.error
      case x: Long   => 0
      case x: Int    => 0
      case Some(x)   => CoordValue(x).error
      case _         => sys.error(s"Not a number ($o) : $oType")
    }
    def toVwe: Vwe = o match {
      case x: Vwe => x
      case _ => Vwe(o.value, o.error)
    }
    def max(b: Any): CoordValue = CoordValue((o, b) match {
      case (_, y: CoordValue)     => this max y.o
      case (x: Long, y: Long)     => x max y
      case (x: Long, y: Int)      => x max y
      case (x: Int, y: Long)      => x.toLong max y
      case (x: Int, y: Int)       => x max y
      case (x: Int, y: Vwe)       => Vwe(x, 0) max y
      case (x: Vwe, y: Int)       => x max Vwe(y, 0)
      case (x: Double, y: Double) => x max y
      case (x: Vwe, y: Vwe)       => Vwe(x.value max y.value, x.error max y.error)
      case (x: Double, y: Vwe)    => Vwe(x, 0) max y
      case (x: Vwe, y: Double)    => x max Vwe(y, 0)
      case (x: Long, y)           => x.toDouble max CoordValue(y).toDouble
      case (x: Int, y)            => x.toDouble max CoordValue(y).toDouble
      case (_, y: Int)            => this max y.toDouble
      case (_, y: Long)           => this max y.toDouble
      case (x, None) if x != None => x
      case (None, x)              => x
      case _                      => sys.error("max of " + o + " : " + oType + " and " + b + " : " + CoordValue(b).oType)
    })
    def min(b: Any): CoordValue = CoordValue((o, b) match {
      case (_, y: CoordValue)     => this min y.o
      case (x: Long, y: Long)     => x min y
      case (x: Long, y: Int)      => x min y
      case (x: Int, y: Long)      => x.toLong min y
      case (x: Int, y: Int)       => x min y
      case (x: Int, y: Vwe)       => Vwe(x, 0) min y
      case (x: Vwe, y: Int)       => x min Vwe(y, 0)
      case (x: Double, y: Double) => x min y
      case (x: Double, y: Vwe)    => y minPoint x
      case (x: Vwe, y)            => x minPoint y
      case (x: Long, y)           => x.toDouble min CoordValue(y).toDouble
      case (x: Int, y)            => x.toDouble min CoordValue(y).toDouble
      case (_, y: Int)            => this min y.toDouble
      case (_, y: Long)           => this min y.toDouble
      case (x, None) if x != None => x
      case (None, x)              => x
      case _                      => sys.error("min of " + o + " : " + oType + " and " + b + " : " + CoordValue(b).oType)
    })
    def isInfinity: Boolean = o match {
      case x: Double => x.isInfinity
      case x: Vwe    => x.value.isInfinity
      case _         => false
    }
    def isNaN: Boolean = o match {
      case x: Double => x.isNaN
      case x: Vwe    => x.value.isNaN
      case _         => false
    }
    def +(b: Any): CoordValue = CoordValue((o, b) match {
      case (_, y: CoordValue)     => this + y.o
      case (x: Long, y: Long)     => x + y
      case (x: Long, y: Int)      => x + y
      case (x: Int, y: Long)      => x.toLong + y
      case (x: Int, y: Int)       => x + y
      case (x: Double, y: Double) => x + y
      case (x: Int, y: Vwe)       => Vwe(x, 0) + y
      case (x: Vwe, y: Int)       => x + Vwe(y, 0)
      case (x: Double, y: Vwe)    => Vwe(x, 0) + y
      case (x: Long, y: Vwe)      => Vwe(x.toDouble, 0) + y
      case (x: Vwe, y: Double)    => x + Vwe(y, 0)
      case (x: Vwe, y: Vwe)       => x + y
      case (_, y: Int)            => this + y.toDouble
      case (_, y: Long)           => this + y.toDouble
      case (x: Long, y)           => x.toDouble + CoordValue(y).toDouble
      case (x: Int, y)            => x.toDouble + CoordValue(y).toDouble
      case (Some(x), Some(y))     => Some((CoordValue(x) + y).noCoordValue)
      case (None, None)           => None
      case _                      => sys.error("+ of " + o + " : " + oType + " and " + b + " : " + CoordValue(b).oType)
    })
    def -(b: Any): CoordValue = CoordValue((o, b) match {
      case (_, y: CoordValue)     => this - y.o
      case (x: Long, y: Long)     => x - y
      case (x: Long, y: Int)      => x - y
      case (x: Int, y: Long)      => x.toLong - y
      case (x: Int, y: Int)       => x - y
      case (x: Double, y: Double) => x - y
      case (x: Int, y: Vwe)       => Vwe(x, 0) - y
      case (x: Vwe, y: Int)       => x - Vwe(y, 0)
      case (x: Double, y: Vwe)    => Vwe(x, 0) - y
      case (x: Long, y: Vwe)      => Vwe(x.toDouble, 0) - y
      case (x: Vwe, y: Double)    => x - Vwe(y, 0)
      case (x: Vwe, y: Vwe)       => x - y
      case (x: Long, y)           => x.toDouble - CoordValue(y).toDouble
      case (x: Int, y)            => x.toDouble - CoordValue(y).toDouble
      case (_, y: Int)            => this - y.toDouble
      case (_, y: Long)           => this - y.toDouble
      case _                      => sys.error("- of " + o + " : " + oType + " and " + b + " : " + CoordValue(b).oType)
    })
    def *(b: Any): CoordValue = CoordValue((o, b) match {
      case (_, y: CoordValue)     => this * y.o
      case (x: Long, y: Long)     => x.toDouble * y
      case (x: Long, y: Int)      => x.toDouble * y
      case (x: Int, y: Long)      => x.toDouble * y
      case (x: Int, y: Int)       => x.toDouble * y
      case (x: Int, y: Vwe)       => Vwe(x, 0) * y
      case (x: Vwe, y: Int)       => x * Vwe(y, 0)
      case (x: Double, y: Double) => x * y
      case (x: Double, y: Vwe)    => Vwe(x, 0) * y
      case (x: Long, y: Vwe)      => Vwe(x.toDouble, 0) * y
      case (x: Vwe, y: Double)    => x * Vwe(y, 0)
      case (x: Vwe, y: Vwe)       => x * y
      case (x: Long, y)           => x.toDouble * CoordValue(y).toDouble
      case (x: Int, y)            => x.toDouble * CoordValue(y).toDouble
      case (_, y: Int)            => this * y.toDouble
      case (_, y: Long)           => this * y.toDouble
      case _                      => sys.error("* of " + o + " : " + oType + " and " + b + " : " + CoordValue(b).oType)
    })
    def /(b: Any): CoordValue = CoordValue((o, b) match {
      case (_, y: CoordValue)     => this / y.o
      case (x: Long, y: Long)     => x.toDouble / y
      case (x: Long, y: Int)      => x.toDouble / y
      case (x: Int, y: Long)      => x.toDouble / y
      case (x: Int, y: Int)       => x.toDouble / y
      case (x: Int, y: Vwe)       => Vwe(x, 0) / y
      case (x: Vwe, y: Int)       => x / Vwe(y, 0)
      case (x: Double, y: Double) => x / y
      case (x: Double, y: Vwe)    => Vwe(x, 0) / y
      case (x: Long, y: Vwe)      => Vwe(x.toDouble, 0) / y
      case (x: Vwe, y: Double)    => x / Vwe(y, 0)
      case (x: Vwe, y: Vwe)       => x / y
      case (x: Long, y)           => x.toDouble / CoordValue(y).toDouble
      case (x: Int, y)            => x.toDouble / CoordValue(y).toDouble
      case (_, y: Int)            => this / y.toDouble
      case (_, y: Long)           => this / y.toDouble
      case _                      => sys.error("/ of " + o + " : " + oType + " and " + b + " : " + CoordValue(b).oType)
    })
    def pow(b: Any): CoordValue = CoordValue((o, b) match {
      case (_, y: CoordValue)     => this pow y.o
      case (x: Vwe, y: Double)    => x pow y
      case (x: Double, y: Double) => math.pow(x, y)
      case _                      => sys.error("pow of " + o + " : " + oType + " and " + b + " : " + CoordValue(b).oType)
    })
    def compare(b: Any): Int = (o, b) match {
      case (_, y: CoordValue)     => this.compare(y.o)
      case (x: Long, y: Long)     => x.compare(y)
      case (x: Long, y: Int)      => x.compare(y)
      case (x: Int, y: Long)      => x.toLong.compare(y)
      case (x: Int, y: Int)       => x.compare(y)
      case (x: Int, y: Vwe)       => Vwe(x, 0) compare y
      case (x: Vwe, y: Int)       => x compare Vwe(y, 0)
      case (x: Double, y: Double) => x.compare(y)
      case (x: Double, y: Vwe)    => Vwe(x, 0).compare(y)
      case (x: Long, y: Vwe)      => Vwe(x.toDouble, 0).compare(y)
      case (x: Vwe, y: Double)    => x.compare(Vwe(y, 0))
      case (x: Vwe, y: Vwe)       => x.compare(y)
      //case (x: Ordered[u], y) if y.isInstanceOf[u] => x.compare(y.asInstanceOf[u])
      case (_, y: Int)                        => this.compare(y.toDouble)
      case (_, y: Long)                       => this.compare(y.toDouble)
      case ((a, b), (c, d))                   => a compare c match {
        case 0 => b compare d
        case x => x
      }
      case ((a, b, c), (d, e, f))             => a compare d match {
        case 0 => (b, c) compare(e, f)
        case x => x
      }
      case (x, y)                             => x.toString.compare(y.toString)
    }
    def <(b: Any) = compare(b) < 0
    def >(b: Any) = compare(b) > 0
    def normalize(b: Any): CoordValue = {
      val v = (o, b) match {
        case (_, cv: CoordValue) => this normalize cv.o
        case (x: Int, _)         => x.toDouble.normalize(b)
        case (x: Long, _)        => x.toDouble.normalize(b)
        case (x: Double, _)      => if (x == 0.0) 0.0 else x / CoordValue(b).value
        case (x: Vwe, _)         => x normalize b
        case (None, _)           => None
        case _                   => sys.error("normalization of " + o + " : " + oType + " and " + b + " : " + CoordValue(b).oType)
      }
      assert(!v.isNaN || (b.isNaN || value.isNaN))
      v
    }
  }

  trait DataPoint extends Serializable {
    def properties: PropertyMap
    def apply(s: String) = properties(s)
    def isDefinedAt(s: String) = properties.isDefinedAt(s)
    def withDefault(s: String, v: => Any) = properties.getOrElse(s, v)
    override def toString = (for ((k, v) <- properties) yield s"$k -> $v").toList.sortWith(_ < _).mkString("{", ",", "}")
  }

  object DataPoint {
    def compare(l: Iterable[DataPoint]) = PropertyMap.findVariations(l map { _.properties })
  }
}
