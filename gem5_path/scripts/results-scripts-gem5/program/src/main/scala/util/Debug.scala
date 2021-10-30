package util

object Debug {
  var enabled = false
  
  @inline 
  def apply(f: => Unit) = if (enabled) f else ()
}
