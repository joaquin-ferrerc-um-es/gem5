package repscr.gem5

import SimulationMix.mixers

object Gem5Properties {
  sealed trait PropertyType
  case object Config extends PropertyType
  case object Result extends PropertyType

  case class Prop(
    kind: PropertyType,
    name: String,
    getter: Simulation.parser.RawGEM5Simulation => Any,
    mixer: SimulationMix.Mixer = mixers.default,
    optional: Boolean = false
  ) {
    knownProperties = knownProperties + (name -> this)
  }

  var knownProperties = Map.empty[String, Prop]

  private implicit class parse(s: String) {
    def parseLong = s.toLong
    def parseOptLong = s match {
      case "" | "None" => None
      case x => x.toLong
    }
    def parseBoolean = s match {
      case "True" => true
      case "False" => false
      case "1" => true
      case "0" => false
      case "true" => true
      case "false" => false
    }
    def parseString: String = s.intern
    def parseDouble: Double = s match {
      case "nan" => Double.NaN
      case x => x.toDouble
    }
    def splitWords: Iterable[String] = s.split(" +")
  }

  private implicit class seqDoubleUtil(s: Iterable[Double]) {
    def average = {
      val l = s.filterNot(_.isNaN)
      l.sum / l.size
    }
  }

  private def check(b: Boolean, msg: String = "Inconsistency parsing results"): Unit = if (!b) throw new RuntimeException(msg)

  /* Configs */

  Prop(Config, "num_cpus", _.configuration("SimulationInfo", "num_cpus").toInt) // TODO: ensure that this matches with the number of cpus in the stats?
  Prop(Config, "protocol", _.configuration("SimulationInfo", "protocol").parseString)
  Prop(Config, "cpu_model", _.configuration("SimulationInfo", "cpu_model").parseString)
  Prop(Config, "benchmark_name", _.configuration("SimulationInfo", "benchmark_name").parseString)
  Prop(Config, "benchmark_size", _.configuration("SimulationInfo", "benchmark_size").parseString)
  Prop(Config, "random_seed", _.configuration("SimulationInfo", "random_seed").parseLong, mixer = mixers.randomSeed)
  Prop(Config, "git_revision", _.configuration("SimulationInfo", "git_revision").parseString)

  Prop(Config, "htm_disable_speculation", _.configuration("SimulationInfo", "htm_disable_speculation").parseBoolean)
  Prop(Config, "htm_binary_suffix", _.configuration("SimulationInfo", "htm_binary_suffix").parseString)
  Prop(Config, "htm_lazy_vm", _.configuration("SimulationInfo", "htm_lazy_vm").parseBoolean)
  Prop(Config, "htm_eager_cd", _.configuration("SimulationInfo", "htm_eager_cd").parseBoolean)
  Prop(Config, "htm_conflict_resolution", _.configuration("SimulationInfo", "htm_conflict_resolution").parseString)
  Prop(Config, "htm_lazy_arbitration", _.configuration("SimulationInfo", "htm_lazy_arbitration").parseString)
  Prop(Config, "htm_allow_read_set_l0_cache_evictions", _.configuration("SimulationInfo", "htm_allow_read_set_l0_cache_evictions").parseBoolean)
  Prop(Config, "htm_allow_read_set_l1_cache_evictions", _.configuration("SimulationInfo", "htm_allow_read_set_l1_cache_evictions").parseBoolean)
  Prop(Config, "htm_allow_write_set_l0_cache_evictions", _.configuration("SimulationInfo", "htm_allow_write_set_l0_cache_evictions").parseBoolean)
  Prop(Config, "htm_allow_write_set_l1_cache_evictions", _.configuration("SimulationInfo", "htm_allow_write_set_l1_cache_evictions").parseBoolean)
  Prop(Config, "htm_allow_read_set_l2_cache_evictions", _.configuration("SimulationInfo", "htm_allow_read_set_l2_cache_evictions").parseBoolean)
  Prop(Config, "htm_allow_write_set_l2_cache_evictions", _.configuration("SimulationInfo", "htm_allow_write_set_l2_cache_evictions").parseBoolean)
  Prop(Config, "htm_precise_read_set_tracking", _.configuration("SimulationInfo", "htm_precise_read_set_tracking").parseBoolean)
  Prop(Config, "htm_allow_load_delaying", _.configuration("SimulationInfo", "htm_allow_load_delaying").parseBoolean)
  Prop(Config, "htm_trans_aware_l0_replacements", _.configuration("SimulationInfo", "htm_trans_aware_l0_replacements").parseBoolean)
  Prop(Config, "htm_reload_if_stale", _.configuration("SimulationInfo", "htm_reload_if_stale").parseBoolean)
  Prop(Config, "htm_l0_downgrade_on_l1_gets", _.configuration("SimulationInfo", "htm_l0_downgrade_on_l1_gets").parseBoolean)
  //Prop(Config, "htm_value_checker", _.configuration("SimulationInfo", "htm_value_checker").parseBoolean)
  //Prop(Config, "htm_isolation_checker", _.configuration("SimulationInfo", "htm_isolation_checker").parseBoolean)
  //Prop(Config, "htm_visualizer", _.configuration("SimulationInfo", "htm_visualizer").parseBoolean)
  Prop(Config, "htm_max_retries", _.configuration("SimulationInfo", "htm_max_retries").parseLong)
  Prop(Config, "htm_backoff", _.configuration("SimulationInfo", "htm_backoff").parseBoolean)
  Prop(Config, "htm_heap_prefault", _.configuration("SimulationInfo", "htm_heap_prefault").parseBoolean)

  /* Results */

  Prop(Result, "simulation_time", _.stats("hostSeconds").parseDouble, mixers.samples)
  Prop(Result, "sim_ticks", _.stats("simTicks").parseLong, mixers.samples)
  // alternatve:  PropertyInfo("cycles_ruby", Result, _.stats("system", "ruby", "cycles").parseLong, mixers.samples)
  // alternatve: PropertyInfo("cycles_cpus", Result, s => (s.stats / "system" / re_cpus /+ "numCycles").map(_.parseLong).max)

  // cache_.+
  Seq((0, "Icache", "l0i"),
    (0, "Dcache", "l0d"),
    (1, "cache", "l1"),
    (2, "L2cache", "l2")).foreach { case (i, gem5Name, ourName) =>
    val re_controllers = s"l${i}_cntrl([0-9]*)".r
    Seq("accesses", "hits", "misses").foreach { stat =>
      Prop(Result, s"cache_${ourName}_${stat}", s => (s.stats / "system" / "ruby" / re_controllers / gem5Name /+ s"demand_${stat}").map(_.parseLong).sum, mixer = mixers.samples)
    }
  }

  // htm_.+
  {
    val re_htm_controllers = s"l0_cntrl([0-9]*)".r
    Prop(Result, "htm_transaction_count_per_cpu", s => (s.stats / "system" / "ruby" /- re_htm_controllers).map {
      case (ctrl, stats) => ctrl -> (stats / "xact_mgr" /+ "htm_transaction_cycles::samples").map(_.parseDouble).getOrElse(0)
    }, mixer = mixers.mapMixer(mixers.samples), optional = true)
    Prop(Result, "htm_transaction_cycles_per_cpu", s => (s.stats / "system" / "ruby" /- re_htm_controllers).map {
      case (ctrl, stats) => ctrl -> (stats / "xact_mgr" /+ "htm_transaction_cycles::mean").map(_.parseDouble).getOrElse(0)
    }, mixer = mixers.mapMixer(mixers.samples), optional = true)

    Prop(Result, "htm_transaction_instructions", s => (s.stats / "system" / "ruby" / re_htm_controllers / "xact_mgr" /+ "htm_transaction_instructions::mean").map(_.parseDouble).average, mixer = mixers.samples, optional = true)
    Prop(Result, "htm_transaction_abort_cause", { s =>
      val r = (s.stats / "system" / "ruby" / re_htm_controllers / "xact_mgr" /+- "htm_transaction_abort_cause::(.+)".r)
        .groupBy(_._1.parseString).view.mapValues(_.map(_._2.splitWords.head.parseLong).sum)
      check(r.isEmpty || r("total") == r.filterKeys(_ != "total").values.sum)
      r.filterKeys(_ != "total").toMap
    }, mixers.mapMixer(mixers.samples), optional = true)
    Prop(Result, "htm_cycles_in_region", { s =>
      val r = (s.stats / "system" / "htm" /+- "cyclesInRegion::(.+)".r)
        .groupBy(_._1.parseString).view.mapValues(_.map(_._2.splitWords.head.parseLong).sum)  // sums all entries with the same key, although there is (or should be) only one in this case.
      check(r.isEmpty || r("total") == r.filterKeys(_ != "total").values.sum)
      r.filterKeys(_ != "total").toMap
    }, mixers.mapMixer(mixers.samples), optional = true)
  }
}

