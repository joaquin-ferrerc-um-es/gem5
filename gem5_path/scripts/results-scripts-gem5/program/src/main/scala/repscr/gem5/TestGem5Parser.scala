package repscr.gem5

import util.misc.{FileCharSequence, RecursiveFileIterator}
import java.io.{File, FileWriter}
import util.time

object TestGem5Parser extends App {
  val dirs_or_files = (if (args.isEmpty) Seq("ins") else args.toSeq) map (new File(_))
  val (dirs, no_dirs) = dirs_or_files.partition(_.isDirectory)
  val files = no_dirs ++ new RecursiveFileIterator(dirs: _*).filter(f => f.getName.endsWith(".stats") || f.getName == "stats.txt")

  files foreach { f =>
    try {
      Simulation.fromFile(f) foreach println
      println()
    } catch {
      case e: Exception =>
        println(s"Exception parsing «$f»: $e")
        e.printStackTrace()
        println()
    }
  }

  //val f = files.head
  //println(f)
  //val s = Simulation.fromFile(f)
  //println(s)

  //val sims = Simulation.parseStatsDirWithProgress(if (args.nonEmpty) args(0) else "ins/")
  //sims foreach (println(_))
  //val mixes = SimulationMix.mixSameConfig(sims)
  //mixes foreach (println(_))

  if (false) files foreach { f =>
    time(s"Copying $f (${f.length} bytes)") {
      val s = new FileCharSequence(f)
      try {
        val g = new File(f.getAbsolutePath + ".2")
        val fw = new FileWriter(g)
        try 0 until s.length foreach (i => fw.append(s.charAt(i)))
        finally fw.close()
      } finally s.close()
    }
  }
}
