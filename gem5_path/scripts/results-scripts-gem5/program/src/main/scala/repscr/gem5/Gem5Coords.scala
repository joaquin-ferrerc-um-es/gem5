package repscr.gem5

import repscr.{PlotCoordinates, Vwe}
import Vwe._
import util.misc.dynamicOrdering
import repscr.points._

object Gem5Coords extends PlotCoordinates[Gem5DataPoint] {

  /* Configs */

  CoordFromProp("num_cpus", doc = "Number of CPUs")
  CoordFromProp("protocol", doc = "Coherence protocol")
  CoordFromProp("cpu_model", doc = "CPU model")
  CoordFromProp("benchmark_name", doc = "Benchmark name", axisTitle = "Benchmark")
  CoordFromProp("benchmark_size", ordering = dynamicOrdering("small", "medium", "large"), doc = "Benchmark problem size")
  CoordFromProp("random_seed", doc = "Random seeds used for this point")
  CoordFromProp("git_revision")

  CoordFromProp("htm_disable_speculation")
  CoordFromProp("htm_binary_suffix")
  CoordFromProp("htm_lazy_vm")
  CoordFromProp("htm_eager_cd")
  CoordFromProp("htm_conflict_resolution", ordering = dynamicOrdering("requester_wins", "requester_stalls_cda_base", "requester_stalls_cda_hybrid"))
  CoordFromProp("htm_lazy_arbitration")
  CoordFromProp("htm_allow_read_set_l0_evictions")
  CoordFromProp("htm_allow_read_set_l1_evictions")
  CoordFromProp("htm_allow_write_set_l0_evictions")
  CoordFromProp("htm_allow_write_set_l1_evictions")
  CoordFromProp("htm_allow_read_set_l2_evictions")
  CoordFromProp("htm_allow_write_set_l2_evictions")
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
  CoordFromProp(s"htm_transaction_count_per_cpu", stacked = true, axisTitle = "Transactions", doc = "Number of transactions per CPU")
  Coord(s"htm_transaction_count", s => s("htm_transaction_count_per_cpu").asMap[Any, Vwe].values.sum, axisTitle = "Transactions", doc = "Total number of transactions")
  CoordFromProp(s"htm_transaction_cycles_per_cpu", stacked = true, axisTitle = "Average cycles per transaction (cycles)", doc = "Average cycles per transaction per CPU")
  Coord(s"htm_transaction_cycles", s => s("htm_transaction_cycles_per_cpu").asMap[Any, Vwe].values.sum, axisTitle = "Average cycles per transaction (cycles)", doc = "Average cycles per transaction")
  CoordFromProp(s"htm_transaction_instructions", axisTitle = "Averge cycles per transaction (cycles)")
  CoordFromProp(s"htm_transaction_abort_cause", stacked = true, axisTitle = "transactions")

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
      case None    => sys.error(s"undefined property: $prop_name")
    }
  }

  implicit class AnyCoordHelper(o: Any) {
    def asMap[K, V] = o.asInstanceOf[Map[K, V]] // TODO: handle other cases if necessary (e.g., lists of pairs)
  }

  /* Project specific coordinates */
  Coord("config_huawei",
        s => (s("htm_binary_suffix"), s("htm_heap_prefault"),
          s("htm_allow_read_set_l0_evictions"), s("htm_allow_read_set_l1_evictions"), s("htm_allow_read_set_l2_evictions"),
          s("htm_trans_aware_l0_replacements"),
          s("htm_lazy_vm"),
          s("htm_conflict_resolution"),
          s("htm_reload_if_stale"),
          s("htm_eager_cd")) match {
          case (".htm.sgl", _, _, _, _, _, _, _, _, _)                                   => "Locks"
          case (".htm.fallbacklock", false, _, _, _, _, true, "requester_wins", _, true)    => "HTM_RW_base"
          case (".htm.fallbacklock", true, false, _, _, _, true, "requester_wins", _, true) => "HTM_RW_+PF"

          case (".htm.fallbacklock", true, true, false, _, _, true, "requester_wins", _, true)        => "HTM_RW+PF+L0RSE"
          case (".htm.fallbacklock", true, true, true, false, false, true, "requester_wins", _, true) => "HTM_RW+PF+L0RSE+L1RSE"
          case (".htm.fallbacklock", true, true, true, true, _, true, "requester_wins", _, true)      => "HTM_RW+PF+L0RSE+L1RSE+L2RSE"

          case (".htm.fallbacklock", true, true, true, false, true, true, "requester_wins", _, true) => "HTM_RW+PF+L0RSE+L1RSE+HAR"

          case (".htm.fallbacklock", true, true, true, false, true, true, "requester_stalls_cda_base", false, true)   => "HTM_CDA+PF+L0RSE+L1RSE+HAR"
          case (".htm.fallbacklock", true, true, true, false, true, true, "requester_stalls_cda_hybrid", false, true) => "HTM_CDAH+PF+L0RSE+L1RSE+HAR"

          case (".htm.fallbacklock", true, true, true, false, true, true, "requester_stalls_cda_hybrid", true, true) => "HTM_CDAH+PF+L0RSE+L1RSE+HAR+RIS"
          case (".htm.fallbacklock", true, true, true, false, true, true, "committer_wins", false, false) => "HTM_LAZYCD+PF+L0RSE+L1RSE+HAR"

          case (".htm.fallbacklock", true, true, true, true, true, false, "requester_stalls_cda_hybrid", true, true) => "LogTM"
        },
        isConfig = true,
        ordering = dynamicOrdering("Locks", "HTM_RW_base",
                                   "HTM_RW_+PF", "HTM_RW+PF+L0RSE", "HTM_RW+PF+L0RSE+L1RSE", "HTM_RW+PF+L0RSE+L1RSE+L2RSE",
                                   "HTM_RW+PF+L0RSE+L1RSE+HAR", "HTM_CDA+PF+L0RSE+L1RSE+HAR", "HTM_CDAH+PF+L0RSE+L1RSE+HAR",
                                   "HTM_CDAH+PF+L0RSE+L1RSE+HAR+RIS", "HTM_LAZYCD+PF+L0RSE+L1RSE+HAR"

                                   ))


}
