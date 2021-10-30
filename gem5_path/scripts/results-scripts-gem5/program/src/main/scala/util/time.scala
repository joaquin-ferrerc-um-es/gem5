package util

object time {
  def now = java.lang.System.currentTimeMillis

  def executionTime[A](f: => A): Long = {
    val start = now
    f
    now - start
  }

  def during[A](t: Long)(f: => A): Unit = {
    val end = now + t
    while (now < end) f
  }

  def apply[A](what: String)(f: => A): A = {
    val start = now
    val r = f
    println(f"${(now - start).toDouble / 1000}%.3fs $what")
    r
  }
}
