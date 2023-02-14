package repscr.gem5

import repscr.gem5.SimulationMix.mixers

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
  Seq[(String, String => Any)](
    ("arch", _.parseString),
    ("num_cpus", _.toInt), // TODO: ensure that this matches with the number of cpus in the stats?
    ("protocol", _.parseString),
    ("cpu_model", _.parseString),
    ("benchmark_name", _.parseString),
    ("benchmark_full_name", _.parseString),
    ("benchmark_size", _.parseString),
    ("git_revision", _.parseString),

    ("config_description_abbrev", _.parseString),

    ("cache_name", _.parseString),
    ("cache_l0i_size", _.parseLong),
    ("cache_l0d_size", _.parseLong),
    ("cache_l0i_assoc", _.parseLong),
    ("cache_l0d_assoc", _.parseLong),
    ("cache_l1i_size", _.parseLong),
    ("cache_l1d_size", _.parseLong),
    ("cache_l1i_assoc", _.parseLong),
    ("cache_l1d_assoc", _.parseLong),
    ("cache_l2_num_caches", _.parseLong),
    ("cache_l2_size_per_cache", _.parseLong),
    ("cache_l2_assoc", _.parseLong),

    ("disable_transparent_hugepages", _.parseBoolean),

    ("network_model", _.parseString),
    ("network_topology", _.parseString),
    ("network_mesh_rows", _.parseLong),

    ("memory_type", _.parseString),
    ("memory_size", _.parseString),

    ("htm_disable_speculation", _.parseBoolean),
    ("htm_binary_suffix", _.parseString),
    ("htm_lazy_vm", _.parseBoolean),
    ("htm_eager_cd", _.parseBoolean),
    ("htm_conflict_resolution", _.parseString),
    ("htm_lazy_arbitration", _.parseString),
    ("htm_allow_read_set_l0_cache_evictions", _.parseBoolean),
    ("htm_allow_read_set_l1_cache_evictions", _.parseBoolean),
    ("htm_allow_write_set_l0_cache_evictions", _.parseBoolean),
    ("htm_allow_write_set_l1_cache_evictions", _.parseBoolean),
    ("htm_allow_read_set_l2_cache_evictions", _.parseBoolean),
    ("htm_allow_write_set_l2_cache_evictions", _.parseBoolean),
    ("htm_precise_read_set_tracking", _.parseBoolean),
    ("htm_allow_load_delaying", _.parseBoolean),
    ("htm_trans_aware_l0_replacements", _.parseBoolean),
    ("htm_reload_if_stale", _.parseBoolean),
    ("htm_l0_downgrade_on_l1_gets", _.parseBoolean),
    ("htm_max_retries", _.parseLong),
    ("htm_backoff", _.parseBoolean),
    ("htm_heap_prefault", _.parseBoolean),
  ).foreach { case (key, parser) =>
    Prop(Config, key, r => parser(r.configuration("SimulationInfo", key)))
  }

  Prop(Config, "random_seed", _.configuration("SimulationInfo", "random_seed").parseLong, mixer = mixers.randomSeed)

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

  // network
  Prop(Result, "network_msg_count", { s =>
    (s.stats / "system" / "ruby" / "network" / "msg_count" /+- "(.+)".r)
      .groupBy(_._1.parseString).view.mapValues(_.map(_._2.splitWords.head.parseLong).sum)
      .toMap
  }, mixers.mapMixer(mixers.samples), optional = true)

  Prop(Result, "network_msg_byte", { s =>
    (s.stats / "system" / "ruby" / "network" / "msg_byte" /+- "(.+)".r)
      .groupBy(_._1.parseString).view.mapValues(_.map(_._2.splitWords.head.parseLong).sum)
      .toMap
  }, mixers.mapMixer(mixers.samples), optional = true)

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
        .groupBy(_._1.parseString).view.mapValues(_.map(_._2.splitWords.head.parseLong).sum) // sums all entries with the same key, although there is (or should be) only one in this case.
      check(r.isEmpty || r("total") == r.filterKeys(_ != "total").values.sum)
      r.filterKeys(_ != "total").toMap
    }, mixers.mapMixer(mixers.samples), optional = true)
  }
}

