/*
    Copyright (C) 2016-2021 Rubén Titos <rtitos@um.es>
    Universidad de Murcia

    GPLv2, see file LICENSE.
*/

#include "mem/ruby/profiler/XactVisualizer.hh"

#include <cassert>
#include <cstdlib>

#include "debug/RubyHTM.hh"
#include "mem/ruby/common/Global.hh"
#include "mem/ruby/profiler/XactProfiler.hh"
#include "mem/ruby/system/Sequencer.hh"


namespace gem5
{

namespace ruby
{

typedef struct {AnnotatedRegion entry; char outp;}XactStateStruct;

XactStateStruct s_xactStateStructMap[AnnotatedRegion_NUM] = {
  // Make sure the order of this map matches annotated_regions.h
  {AnnotatedRegion_DEFAULT, '%'},
  {AnnotatedRegion_BARRIER, '+'},
  {AnnotatedRegion_TRANSACTIONAL, '$'},
  {AnnotatedRegion_TRANSACTIONAL_COMMITTED, 0},
  {AnnotatedRegion_TRANSACTIONAL_ABORTED, 0},
  {AnnotatedRegion_COMMITTING, 'C'},
  {AnnotatedRegion_ABORTING, 'X'},
  {AnnotatedRegion_ABORT_HANDLER_HASLOCK, 'L'},
  {AnnotatedRegion_ABORT_HANDLER, 'x'},
  {AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_THRESHOLD, 'r'},
  {AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_EXCEPTION, 'e'},
  {AnnotatedRegion_ABORT_HANDLER_WAITFORRETRY_SIZE, 'c'},
  {AnnotatedRegion_KERNEL, 'k'},
  {AnnotatedRegion_KERNEL_HASLOCK, 'K'},
  {AnnotatedRegion_STALLED, '|'},
  {AnnotatedRegion_STALLED_COMMITTED, 0},
  {AnnotatedRegion_STALLED_ABORTED, 0},
  {AnnotatedRegion_STALLED_NONTRANS, '-'},
  {AnnotatedRegion_INVALID, '!'},
};

XactVisualizer::XactVisualizer(XactProfiler* profiler, std::ostream* output) {
    // By default, dump to stderr
    m_xact_visualizer_output_file_ptr = output;
    m_xact_profiler = profiler;
}

XactVisualizer::~XactVisualizer() {
}


void XactVisualizer::printAnnotatedRegions(std::string& extraStr)
{

  for (int i = 0; i < m_xact_profiler->m_annotatedRegion.size(); i++)
    (* m_xact_visualizer_output_file_ptr) <<
      s_xactStateStructMap[m_xact_profiler->m_annotatedRegion[i]].outp << " ";
  (* m_xact_visualizer_output_file_ptr) << "   " <<
      curTick() << "   " << std::setw(12)  <<
      g_system_ptr->curCycle()-g_system_ptr->getStartCycle()  <<
      extraStr << std::endl;
}

} // namespace ruby
} // namespace gem5
