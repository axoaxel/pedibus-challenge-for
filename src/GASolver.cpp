// Copyright 2017 <Biagio Festa>
#include <memory>
#include <cassert>
#include <vector>
#include "GASolver.hpp"

namespace for_ch {

const GASolver* GASolver::mps_running_solver = nullptr;
const std::vector<bool>* GASolver::mps_hint = nullptr;

GASolver::GASolver(const std::shared_ptr<ProblemDatas>& problem) noexcept :
    mp_problem(problem) {
}

std::vector<bool> GASolver::run(
    int argc, char** argv, const std::vector<bool>* hint) {
  assert(argv != nullptr);

  // Set global pointer as this solver
  mps_running_solver = this;

  // Get the numer of edges
  const unsigned num_edges = mp_problem->m_numEdges;

  // Set hit if any
  mps_hint = (hint != nullptr ? hint : nullptr);

  // Initalize Random seeds
  GARandomSeed();

  // Set default parameters
  set_default_parameter();

  // Initialize a genome
  Genome genome(num_edges, &GASolver::ga_genome_fitness);
  genome.initializer(&GASolver::ga_genome_init);
  genome.mutator(&GASolver::ga_genome_mutator);
  if (m_custom_crossover) {
    genome.crossover(&GASolver::ga_genome_crossover);
  }

  // Initialize genetic algorithm
  GASimpleGA ga(genome);
  ga.set(gaNpopulationSize, m_sizePopulation);
  ga.set(gaNpCrossover, m_pCrossover);
  ga.set(gaNpMutation, m_pMutation);
  ga.set(gaNnGenerations, 100000);
  ga.minimize();
  ga.initialize();
  ga.terminator(&GASolver::ga_algorithm_terminator);

  // Save log on disk for verbose option
  if (m_verbose) {
    ga.scoreFilename("log_genetic.txt");
    ga.selectScores(GAStatistics::Minimum |
                    GAStatistics::Mean |
                    GAStatistics::Maximum |
                    GAStatistics::Deviation |
                    GAStatistics::Diversity);
    ga.scoreFrequency(1);
    ga.flushFrequency(10);
  }

  // Override parameters from cmd
  ga.parameters(argc, argv);

  // Set the start time
  m_time_start = Clock::now();

  // Run algorithm
  print_ga_parameters(ga, &std::cout);

  if (m_displayInfo == true) {
    while (ga.done() != gaTrue) {
      print_current_ga_state(ga, &std::cout);
      ga.step();
    }
  } else {
    ga.evolve();
  }

  const auto& result_stats = ga.statistics();
  const Genome& best_result = (const Genome&) result_stats.bestIndividual();

  // Construct best solution
  std::vector<bool> best_solution(mp_problem->m_numEdges, false);
  for (EdgeIndex e = 0; e < mp_problem->m_numEdges; ++e) {
    if (best_result.gene(e) == 1) {
      assert(e < best_solution.size());
      best_solution[e] = true;
    }
  }

  return best_solution;
}

void GASolver::print_current_ga_state(const GAGeneticAlgorithm& ga,
                                      std::ostream* os) const noexcept {
  assert(os != nullptr);
}

void GASolver::ga_genome_init(GAGenome& g) noexcept {
  Genome& genome = (Genome&) g;
  if (mps_hint != nullptr) {
    mps_running_solver->init_genome_w_solution<Genome>(
        &genome, *mps_hint);
  } else {
    mps_running_solver->init_genome_w_trivial_solution<Genome>(&genome);
  }

#ifndef NDEBUG
  unsigned num_leaves;
  assert(mps_running_solver->is_feasible<Genome>(
      genome, &num_leaves, nullptr) == FEASIBLE);
#endif
}

float GASolver::ga_genome_fitness(GAGenome& g) noexcept {
  Genome& genome = (Genome&) g;
  return mps_running_solver->evaluator_genome<Genome>(genome);
}

int GASolver::ga_genome_mutator(GAGenome& g, float mp) noexcept {
  Genome& genome = (Genome&) g;
  return mps_running_solver->mutator_genome<Genome>(&genome, mp);
}

int GASolver::ga_genome_crossover(const GAGenome& dad,
                                  const GAGenome& mom,
                                  GAGenome* bro,
                                  GAGenome* sis) noexcept {
  assert(bro != nullptr);
  assert(sis != nullptr);
  Genome& genome_dad = (Genome&) dad;
  Genome& genome_mom = (Genome&) mom;
  Genome& genome_bro = dynamic_cast<Genome&>(*bro);
  Genome& genome_sis = dynamic_cast<Genome&>(*sis);

  return mps_running_solver->crossover_genome(genome_dad, genome_mom,
                                              &genome_bro, &genome_sis);
}

GABoolean GASolver::ga_algorithm_terminator(GAGeneticAlgorithm & ga) noexcept {
  using Resolution = std::chrono::seconds;

  const auto time_now = Clock::now();
  const auto time_elapsed =
      time_now - mps_running_solver->m_time_start;

  const unsigned elapsed_count =
      std::chrono::duration_cast<Resolution>(time_elapsed).count();

  if (elapsed_count > mps_running_solver->m_timeMax_seconds) {  // 10 minutes
    return gaTrue;
  }

  return gaFalse;
}

void GASolver::print_ga_parameters(const GAGeneticAlgorithm& ga,
                                  std::ostream* os) const noexcept {
  assert(os != nullptr);
  *os << "----------------------------------------\n"
      << "GENETIC ALGORITHM PARAMETERS:\n"
      << "Population Size: " << ga.populationSize() << "\n"
      << "Num. Generations: " << ga.nGenerations() << "\n"
      << "Num. Convergence: " << ga.nConvergence() << "\n"
      << "Prob. Convergence: " << ga.pConvergence() << "\n"
      << "Prob. Mutation: " << ga.pMutation() << "\n"
      << "Prob. Crossover: " << ga.pCrossover() << "\n"
      << "Custom Crossover: " << m_custom_crossover << "\n"
      << "Max time [s]: " << m_timeMax_seconds << "\n"
      << "-----------------------------------------\n";
  os->flush();
}

void GASolver::set_default_parameter() noexcept {
  const unsigned num_nodes = mp_problem->m_numNodes;
  if (num_nodes <= 11) {
    m_pCrossover = 0.9f;
    m_pMutation = 0.05f;
    m_sizePopulation = 10;
  } else if (num_nodes <= 21) {
    m_pCrossover = 0.7f;
    m_pMutation = 0.002f;
    m_sizePopulation = 4;
  } else if (num_nodes <= 31) {
    m_pCrossover = 0.9f;
    m_pMutation = 0.05f;
    m_sizePopulation = 4;
  } else if (num_nodes <= 51) {
    m_pCrossover = 0.9f;
    m_pMutation = 0.05f;
    m_sizePopulation = 4;
  } else if (num_nodes <= 81) {
    m_pCrossover = 0.7f;
    m_pMutation = 0.02f;
    m_sizePopulation = 6;
  } else if (num_nodes <= 101) {
    m_pCrossover = 0.7f;
    m_pMutation = 0.02f;
    m_sizePopulation = 6;
  } else if (num_nodes <= 151) {
    m_pCrossover = 0.7f;
    m_pMutation = 0.04f;
    m_sizePopulation = 6;
  } else if (num_nodes <= 201) {
    m_pCrossover = 0.7f;
    m_pMutation = 0.04f;
    m_sizePopulation = 6;
  } else if (num_nodes <= 251) {
    m_pCrossover = 0.7f;
    m_pMutation = 0.02f;
    m_sizePopulation = 12;
  } else if (num_nodes <= 301) {
    m_pCrossover = 0.9f;
    m_pMutation = 0.01f;
    m_sizePopulation = 8;
  }
}

}  // namespace for_ch
