package repscr.gem5

import repscr.Vwe
import repscr.plots.PlotCoordinates
import util.misc.dynamicOrdering
import repscr.points._

object Gem5Coords extends PlotCoordinates[Gem5DataPoint] {

  /* Configs */

  CoordFromProp("arch", doc = "ISA")
  CoordFromProp("num_cpus", doc = "Number of CPUs")
  CoordFromProp("protocol", doc = "Coherence protocol")
  CoordFromProp("cpu_model", doc = "CPU model")
  CoordFromProp("benchmark_name", doc = "Benchmark name", axisTitle = "Benchmark")
  CoordFromProp("benchmark_full_name", doc = "Benchmark name", axisTitle = "Benchmark")
  CoordFromProp("benchmark_size", ordering = dynamicOrdering("small", "medium", "large"), doc = "Benchmark problem size")
  CoordFromProp("random_seed", doc = "Random seeds used for this point")
  CoordFromProp("git_revision")

  CoordFromProp("config_description_abbrev")

  CoordFromProp("cache_name")
  Coord("cache_l0i", s => s"${s("cache_l0i_size")}-${s("cache_l0i_assoc")}w", isConfig = true)
  Coord("cache_l0d", s => s"${s("cache_l0d_size")}-${s("cache_l0d_assoc")}w", isConfig = true)
  Coord("cache_l1i", s => s"${s("cache_l1i_size")}-${s("cache_l1i_assoc")}w", isConfig = true)
  Coord("cache_l1d", s => s"${s("cache_l1d_size")}-${s("cache_l1d_assoc")}w", isConfig = true)
  Coord("cache_l2", s => s"${s("cache_l2_num_caches")}×${s("cache_l2_size_per_cache")}-${s("cache_l2_assoc")}w", isConfig = true)

  CoordFromProp("disable_transparent_hugepages")

  CoordFromProp("network_model")

  CoordFromProp("memory_type")
  CoordFromProp("memory_size")

  CoordFromProp("htm_disable_speculation")
  CoordFromProp("htm_binary_suffix")
  CoordFromProp("htm_lazy_vm")
  CoordFromProp("htm_eager_cd")
  CoordFromProp("htm_conflict_resolution", ordering = dynamicOrdering("requester_wins", "requester_stalls_cda_base", "requester_stalls_cda_hybrid"))
  CoordFromProp("htm_lazy_arbitration")
  CoordFromProp("htm_allow_read_set_l0_cache_evictions")
  CoordFromProp("htm_allow_read_set_l1_cache_evictions")
  CoordFromProp("htm_allow_write_set_l0_cache_evictions")
  CoordFromProp("htm_allow_write_set_l1_cache_evictions")
  CoordFromProp("htm_allow_read_set_l2_cache_evictions")
  CoordFromProp("htm_allow_write_set_l2_cache_evictions")
  CoordFromProp("htm_precise_read_set_tracking")
  CoordFromProp("htm_allow_load_delaying")
  CoordFromProp("htm_trans_aware_l0_replacements")
  CoordFromProp("htm_reload_if_stale")
  CoordFromProp("htm_l0_downgrade_on_l1_gets")
  //CoordFromProp("htm_value_checker")
  //CoordFromProp("htm_isolation_checker")
  //CoordFromProp("htm_visualizer")
  CoordFromProp("htm_max_retries")
  CoordFromProp("htm_backoff")
  CoordFromProp("htm_heap_prefault")

  Coord("files", _.files, doc = "All files that were parsed to generate this data point")
  Coord("num_files", _.files.size, doc = "Number of files that were parsed to generate this data point")

  /* Results */

  CoordFromProp("simulation_time", axisTitle = "Simulation time (seconds)", doc = "Simulation time in seconds")
  Coord("cycles_ticks", _ ("sim_ticks") / 500, axisTitle = "Execution time (cycles)")
  // TODO: Coord("cycles", _ ("cycles_ruby"), axisTitle = "Execution time (cycles)")

  // cache_.+
  Seq("l0i", "l0d", "l1", "l2").foreach { cache =>
    CoordFromProp(s"cache_${cache}_accesses", axisTitle = s"Accesses to ${cache}")
    CoordFromProp(s"cache_${cache}_hits", axisTitle = s"Hits to ${cache}")
    CoordFromProp(s"cache_${cache}_misses", axisTitle = s"Misses to ${cache}")
    Coord(s"cache_${cache}_miss_rate", s => s("cache_${cache}_misses") / s(s"cache_${cache}_accesses"), axisTitle = s"Miss rate to ${cache}")
  }

  // htm_.+
  CoordFromProp("htm_transaction_count_per_cpu", stacked = true, axisTitle = "Transactions", doc = "Number of transactions per CPU")
  Coord("htm_transaction_count", s => s("htm_transaction_count_per_cpu").asMap[Any, Vwe].values.sum, axisTitle = "Transactions", doc = "Total number of transactions")
  CoordFromProp(s"htm_transaction_cycles_per_cpu", stacked = true, axisTitle = "Average cycles per transaction (cycles)", doc = "Average cycles per transaction per CPU")
  Coord("htm_transaction_cycles", s => s("htm_transaction_cycles_per_cpu").asMap[Any, Vwe].values.sum, axisTitle = "Average cycles per transaction (cycles)", doc = "Average cycles per transaction")
  CoordFromProp("htm_transaction_instructions", axisTitle = "Averge cycles per transaction (cycles)")
  CoordFromProp("htm_transaction_abort_cause", stacked = true, axisTitle = "transactions")
  CoordFromProp("htm_cycles_in_region", stacked = true, axisTitle = "cycles")

  def addSimulationsDependentCoords(simulations: Iterable[Gem5DataPoint]): Unit = {
    /* none */
  }

  override def defaultStringToCoord(s: String) = {
    val isConfig = Gem5Properties.knownProperties get s exists (_.kind == Gem5Properties.Config)
    Coord(s, { p => if (p.isDefinedAt(s)) p(s) else s"undefined coordinate «$s»" }, isConfig = isConfig)
  }

  def CoordFromProp(
    prop_name: String,
    stacked: Boolean = false,
    axisTitle: String = "????",
    ordering: Ordering[Any] = dynamicOrdering(),
    doc: String = "???") = {
    Gem5Properties.knownProperties.get(prop_name) match {
      case Some(p) => Coord(p.name, _ (p.name), isConfig = p.kind == Gem5Properties.Config, stacked = stacked, axisTitle = axisTitle, ordering = ordering, doc = doc)
      case None => sys.error(s"undefined property: $prop_name")
    }
  }

  implicit class AnyCoordHelper(o: Any) {
    def asMap[K, V] = o.asInstanceOf[Map[K, V]] // TODO: handle other cases if necessary (e.g., lists of pairs)
  }

  /* Project specific coordinates. They are present here temporarily while they may be useful when using the web UI. TODO: move to a specific file once a project is closed */
  Coord("config_cost_effective",
    s => (s("htm_binary_suffix"), s("htm_heap_prefault"),
      s("htm_allow_read_set_l0_cache_evictions"), s("htm_allow_read_set_l1_cache_evictions"), s("htm_allow_read_set_l2_cache_evictions"),
      s("htm_trans_aware_l0_replacements"),
      s("htm_lazy_vm"),
      s("htm_conflict_resolution"),
      s("htm_reload_if_stale"),
      s("htm_eager_cd"),
      s("htm_lazy_arbitration")) match {
      case (".htm.sgl", _, _, _, _, _, _, _, _, _, _) => "locks"
      case (".htm.fallbacklock", false, _, _, _, _, true, "requester_wins", _, true, _) => "base_nopf"
      case (".htm.fallbacklock", true, false, _, _, _, true, "requester_wins", _, true, _) => "base"

      case (".htm.fallbacklock", true, true, false, _, _, true, "requester_wins", _, true, _) => "l2rs"
      case (".htm.fallbacklock", true, true, true, false, false, true, "requester_wins", _, true, _) => "l3rs"
      case (".htm.fallbacklock", true, true, true, true, _, true, "requester_wins", _, true, _) => "lxrs"

      case (".htm.fallbacklock", true, true, true, false, true, true, "requester_wins", _, true, _) => "l3rs_l1rpl"

      case (".htm.fallbacklock", true, true, true, false, true, true, "requester_stalls_cda_base", false, true, _) => "l3rs_l1rpl_reqstallb"
      case (".htm.fallbacklock", true, true, true, false, true, true, "requester_stalls_cda_hybrid", false, true, _) => "l3rs_l1rpl_reqstallh"

      case (".htm.fallbacklock", true, true, true, false, true, true, "requester_stalls_cda_hybrid", true, true, _) => "l3rs_l1rpl_reqstallh_precrs"
      case (".htm.fallbacklock", true, true, true, false, true, true, "committer_wins", false, false, "magic") => "l3rs_l1rpl_lazycd_magic"
      case (".htm.fallbacklock", true, true, true, false, true, true, "committer_wins", false, false, "token") => "l3rs_l1rpl_lazycd"

      case (".htm.fallbacklock", true, true, true, true, true, false, "requester_stalls_cda_hybrid", true, true, _) => "lxrs_l1rpl_reqstallh_precrs_log"
      case (MissingProperty, _, _, _, _, _, _, _, _, _, _) => "NoHTM"
    },
    isConfig = true,
    ordering = dynamicOrdering("locks", "base_nopf",
      "base", "l2rs", "l3rs", "lxrs",
      "l3rs_l1rpl", "l3rs_l1rpl_reqstallb", "l3rs_l1rpl_reqstallh",
      "l3rs_l1rpl_reqstallh_precrs", "l3rs_l1rpl_lazycd"

    ))


}
