package util

import language.implicitConversions
import java.io.File
import java.nio.CharBuffer
import scala.collection.parallel.CollectionConverters._

import scala.annotation.tailrec

object misc {
  class FileLinesIterator(file: File) extends Iterator[String] {
    def this(fname: String) = this(new File(fname))
    val f = new java.io.BufferedReader(new java.io.FileReader(file))
    var n = f.readLine()
    def next() = {
      val r = n
      n = f.readLine()
      if (n == null) {
        f.close()
      }
      r
    }
    def hasNext = n != null
  }

  class RecursiveFileIterator(files: File*) extends Iterator[File] {
    def this(fname: String) = this(new File(fname))
    var stack = (files filter (f => f.isDirectory && !f.listFiles.isEmpty) map (_.listFiles.iterator)).toList
    def hasNext = stack.nonEmpty
    def next() = {
      val n = stack.head.next()
      if (!stack.head.hasNext) {
        stack = stack.tail
      }
      if (n.isDirectory && n.listFiles != null && !n.listFiles.isEmpty) {
        stack = n.listFiles.iterator :: stack
      }
      n
    }
  }

  class SubSequence(charSequence: CharSequence, start: Int, end: Int) extends CharSequence {
    val length: Int = end - start
    def charAt(index: Int): Char = charSequence.charAt(start + index)
    def subSequence(newstart: Int, newend: Int): CharSequence = new SubSequence(charSequence, newstart + start, newend + start)
    override def toString = (new StringBuilder ++= start until (length + start) map charSequence.charAt).result()
  }

  class FileCharSequence(file: File, charset: String = "UTF-8") extends CharSequence {
    import java.io.RandomAccessFile
    import java.nio.channels.FileChannel
    import java.nio.charset.Charset

    val pageSize = 16 * 1024
    var mappings = collection.mutable.ArrayBuffer(0)
    var charsBuffer = CharBuffer.allocate(pageSize)
    var charsOffset = -1
    var charsLimit = -1
    def charsPage = charsOffset / pageSize
    var atEndPage = false
    val decoder = Charset.forName(charset).newDecoder
    val input = new RandomAccessFile(file, "r")
    val channel = input.getChannel
    val bytes = channel.map(FileChannel.MapMode.READ_ONLY, 0, channel.size.toInt)

    def doReadPage(page: Int): Unit = {
      bytes.position(mappings(page))
      charsBuffer.clear()
      val cr = decoder.decode(bytes, charsBuffer, true)
      if (cr.isError) cr.throwException() // TODO: handle errors better
      charsOffset = page * pageSize
      charsLimit = charsOffset + charsBuffer.position()
      atEndPage = (bytes.position: Int) == (bytes.limit: Int)
      assert(charsLimit - charsOffset <= pageSize)
      assert(charsLimit - charsOffset == pageSize || atEndPage)
      charsBuffer.rewind()
      assert(mappings.size == page + 1 || mappings(page + 1) == (bytes.position: Int))
      if (mappings.size == page + 1 && !atEndPage) {
        mappings.append(bytes.position)
      }
    }
    def readPage(page: Int): Unit = {
      while (mappings.size < page && !atEndPage) {
        doReadPage(mappings.size)
      }
      if (charsPage != page) {
        doReadPage(page)
      }
    }
    doReadPage(0)

    lazy val length: Int = {
      var last = mappings.size - 1
      if (charsPage != last) {
        readPage(last)
      }
      assert((bytes.limit: Int) == file.length) // FIXME: only if ascii
      while (!atEndPage) {
        last = last + 1
        readPage(last)
      }
      assert(charsLimit <= file.length.toInt)
      charsLimit
    }

    def readOffset(offset: Int) = readPage(offset / pageSize)

    def charAt(index: Int): Char = {
      readOffset(index)
      charsBuffer.get(index - charsOffset)
    }

    def subSequence(start: Int, end: Int): CharSequence = new SubSequence(this, start, end)

    def close(): Unit = {
      channel.close()
      input.close()
    }

    override def toString = (new StringBuilder ++= 0 until length map charAt).result()
  }

  val AsStringOrdering = Ordering.fromLessThan[Any] { (a, b) => a.toString < b.toString }
  def SeqOrdering[A, S <: Iterable[A]](ords: Iterable[Ordering[A]]) =
    Ordering.fromLessThan[S] { (a: S, b: S) =>
      val ai = a.iterator
      val bi = b.iterator
      val ordsi = ords.iterator

      @tailrec
      def it: Boolean =
        if (ai.hasNext && bi.hasNext && ordsi.hasNext) {
          val ae = ai.next()
          val be = bi.next()
          val elemord = ordsi.next()
          if (elemord.lt(ae, be)) true
          else if (elemord.lt(be, ae)) false
          else it
        } else {
          a.size < b.size
        }

      it
    }
  def SeqHomogeneousOrdering[A, S <: Iterable[A]](implicit ord: Ordering[A]): Ordering[S] = SeqOrdering(LazyList.continually(ord))

  def printToFile[T](fname: String)(thunk: => T): T = printToFile(new File(fname))(thunk)
  def printToFile[T](f: File)(thunk: => T): T = {
    val s = new java.io.FileOutputStream(f)
    try Console.withOut(s)(thunk)
    finally s.close()
  }

  class StringBuilderStream extends java.io.OutputStream {
    val sb = new StringBuilder
    def write(b: Int): Unit = sb.append(b.toChar)
    override def toString = sb.toString
  }

  implicit class RichIterable[A](l: Iterable[A]) {
    def self = l

    def allEquals = {
      if (l.isEmpty) true
      else {
        val it = l.iterator
        val first = it.next()
        it forall (first == _)
      }
    }

    def countItems: Iterable[(A, Int)] = {
      val s = collection.mutable.Map.empty[A, Int].withDefaultValue(0)
      l foreach { i => s(i) = s(i) + 1 }
      val ret = new collection.mutable.ArrayBuffer[(A, Int)]
      l foreach { i =>
        if (s(i) > 0) {
          ret += (i -> s(i))
          s(i) = 0
        }
      }
      ret
    }

    def countItemsSorted: Seq[(A, Int)] = {
      val s = collection.mutable.Map.empty[A, Int].withDefaultValue(0)
      l foreach { i => s(i) = s(i) + 1 }
      s.sortWith((x, y) => repscr.points.CoordValue(x._1) < y._1)
    }

    def sortWith(lt: (A, A) => Boolean): Seq[A] = l.toSeq sortWith lt

    def sortAsStrings = sortWith(_.toString < _.toString)

    def sortAsAny = sortWith { (x, y) => repscr.points.CoordValue(x) < y }

    def filterDuplicates: Seq[A] = new Iterator[A] {
      val seen = new collection.mutable.HashSet[A]
      var i = l.iterator
      def next() = {
        val ret = i.next()
        seen += ret
        i = i.dropWhile(seen(_))
        ret
      }
      def hasNext = i.hasNext
    }.toSeq

    def filterDuplicatesFunc[B](fn: A => B): Iterator[A] = new Iterator[A] {
      val seen = new collection.mutable.HashSet[B]
      var i = l.iterator
      def next() = {
        val ret = i.next()
        seen += fn(ret)
        i = i.dropWhile { x => seen(fn(x)) }
        ret
      }
      def hasNext = i.hasNext
    }
  }

  implicit class RichInputStream(val is: java.io.InputStream) {
    def copyToFile(f: File): Unit = {
      val os = new java.io.FileOutputStream(f)
      val buffer = new Array[Byte](1024 * 4)
      var r = is.read(buffer)
      while (r >= 0) {
        os.write(buffer, 0, r)
        r = is.read(buffer)
      }
      os.close()
      is.close()
    }

    def readAndDiscard(): Unit = {
      val buffer = new Array[Byte](1024 * 4)
      var r = is.read(buffer)
      while (r >= 0) {
        r = is.read(buffer)
      }
      is.close()
    }

    def readAsString(): String = {
      val out = new StringBuilder
      val buffer = new Array[Char](1024 * 4)
      val in = new java.io.InputStreamReader(is, "UTF-8")
      var r = in.read(buffer)
      while (r >= 0) {
        out.appendAll(buffer, 0, r)
        r = in.read(buffer)
      }
      in.close()
      out.result()
    }
  }

  def createDirs(d: File): Unit = {
    if (!d.exists()) {
      if (d.getParentFile != null) createDirs(d.getParentFile)
      if (!d.mkdir()) sys.error(s"No se puede crear «$d»")
    }
  }
  def createDirs(d: String): Unit =
    createDirs(new File(d))

  def dynamicLessThan(preferredOrer: Any*): ((Any, Any) => Boolean) = {
    val prefOrd = (preferredOrer map {
      case s: String => s.toLowerCase
      case x => x
    }).zipWithIndex.toMap.withDefaultValue(-1)

    def tryPasrseAsNumberWithSuffix(s: String): (Option[Double], String) = {
      val (prefix, suffix) = s span { c => c.isDigit || c == '.' }
      try ((Some(prefix.toDouble), suffix))
      catch {
        case _: Throwable =>
          if (s.contains("..")) { // handle .. as a separator too
            val i = s.indexOf("..")
            try ((Some(s.substring(0, i).toDouble), s.substring(i)))
            catch {
              case _: Throwable => (None, s)
            }
          } else {
            (None, s)
          }
      }
    }

    @scala.annotation.tailrec
    def compareItem(a: String, b: String): Int = {
      val ai = prefOrd(a)
      val bi = prefOrd(b)
      (ai, bi) match {
        case (-1, -1) =>
          val (na, sa) = tryPasrseAsNumberWithSuffix(a)
          val (nb, sb) = tryPasrseAsNumberWithSuffix(b)
          (na, nb) match {
            case (None, None) => a compare b
            case (Some(x), None) => -1
            case (None, Some(x)) => 1
            case (Some(x), Some(y)) if x == y => compareItem(sa, sb)
            case (Some(x), Some(y)) => x compare y
          }
        case (-1, x) => 1
        case (x, -1) => -1
        case (x, y) => x compare y
      }
    }

    @scala.annotation.tailrec
    def ltList(as: List[String], bs: List[String]): Boolean = (as, bs) match {
      case (Nil, Nil) => false
      case (Nil, _ :: _) => true
      case (_ :: _, Nil) => false
      case (a1 :: at, b1 :: bt) =>
        compareItem(a1, b1).sign match {
          case -1 => true
          case 1 => false
          case _ => ltList(at, bt)
        }
    }

    def dynLT(a: Any, b: Any): Boolean = {
      (a, b) match {
        case ((ax, ay), (bx, by)) =>
          if (dynLT(ax, bx)) true
          else if (dynLT(bx, ax)) false
          else dynLT(ay, by)
        case _ =>
          val as = a.toString.toLowerCase
          val bs = b.toString.toLowerCase
          val ai = prefOrd(as)
          val bi = prefOrd(bs)
          if (ai != -1 && bi != -1) ai < bi
          else ltList(as.split("-|\\+|,").toList, bs.split("-|\\+|,").toList)
      }
    }

    dynLT
  }

  def dynamicOrdering(preferredOrer: Any*): Ordering[Any] = Ordering.fromLessThan(dynamicLessThan(preferredOrer: _*))

  implicit class RichRegex(re: scala.util.matching.Regex) {
    def matches(s: String) = re.pattern.matcher(s).matches
  }

  class Cache[A, B](fn: A => B) extends (A => B) {
    var cache = Map.empty[A, B]
    def apply(k: A): B = cache get k match {
      case Some(v) => v
      case None =>
        val r = fn(k)
        cache += (k -> r)
        r
    }
    def seed(vs: Iterable[A]): Unit = vs foreach this
  }
  def Cache[A, B](fn: A => B) = new Cache(fn)

  import concurrent._

  class ConcurrentCache[A, B](fn: A => B)(implicit executor: ExecutionContext) extends (A => B) {
    sealed trait CacheEntry
    case class Ready(f: Future[B]) extends CacheEntry
    case object Busy extends CacheEntry
    var cache = new java.util.concurrent.atomic.AtomicReference(Map.empty[A, CacheEntry])
    def apply(k: A): B = {
      var r: Option[B] = None
      while (r.isEmpty) {
        val m = cache.get
        m get k match {
          case None => {
            val n = m + (k -> Busy)
            if (cache.compareAndSet(m, n)) {
              val f = Future {
                fn(k)
              }
              while ( {
                val m = cache.get
                val n = m + (k -> Ready(f))
                !cache.compareAndSet(m, n)
              }) {}
            }
          }
          case Some(Busy) =>
          case Some(Ready(f)) => {
            r = Some(Await.result(f, duration.Duration.Inf))
          }
        }
      }
      r.get
    }
    def seed(vs: Iterable[A]): Unit = vs.par foreach this
  }
  def ConcurrentCache[A, B](fn: A => B)(implicit executor: ExecutionContext) = new ConcurrentCache(fn)

  def reduceMaps[K, V](maps: Map[K, V]*)(f: (V, V) => V, missingValue: V): Map[K, V] = {
    val keys = maps.flatMap(_.keys).toSet
    keys.map(k => k -> maps.map(_.getOrElse(k, missingValue)).reduce(f)).toMap
  }

  def regroupMap[K, M <: Map[K, Any]](m: M)(regroupBy: K => K) = m.groupBy(i => regroupBy(i._1)).view.mapValues(
    _.values.reduce { (a, b) => (repscr.points.CoordValue(a) + b).noCoordValue }
  ).toMap
}
