/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef MPAODV_QTABLE_H
#define MPAODV_QTABLE_H

#include "mpaodv-rtable.h"

#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"

#include <deque>
#include <map>
#include <vector>

namespace ns3
{
namespace mpaodv
{

/**
 * \ingroup mpaodv
 * \brief One route (next hop) to a destination with its Q-value.
 */
struct QRecord
{
    RoutingTableEntry rt; ///< route
    double qValue;        ///< Q-value
    uint32_t txCount;     ///< number of Q-updates
    uint32_t ackCount;    ///< number of acknowledged frames

    QRecord()
        : qValue(0.0),
          txCount(0),
          ackCount(0)
    {
    }

    /**
     * \param e route
     * \param q Q-value
     */
    QRecord(const RoutingTableEntry& e, double q)
        : rt(e),
          qValue(q),
          txCount(0),
          ackCount(0)
    {
    }
};

/**
 * \ingroup mpaodv
 * \brief Routes per destination (at most MaxPaths), next-hop selection and Q-learning.
 *
 * Selection: PMAODV, p_i = (1/HC_i) / sum_k (1/HC_k); QMAODV, SA-QMAODV, epsilon-greedy on Q.
 * Initial Q (QMAODV Eq. 1): Q_i = (1/HC_i) / sum_k (1/HC_k).
 * Update: Q <- (1 - alpha) Q + alpha (r + gamma max Q), r = w1 ACK + w2 / (1 + delay_ms) + w3 E.
 * SA-QMAODV: epsilon <- min(0.5, epsilon + 0.2) on a route error, max(0.1, epsilon - 0.02) per
 * tick; alpha = 0.1 + 0.8 (1 - exp(-lambda DeltaSeq)); weights (0.5, 0.4, 0.1), or
 * (0.1, 0.1, 0.8) when the residual energy of the node is below the threshold.
 */
class QTable
{
  public:
    /**
     * \param maxPaths maximum number of routes per destination
     */
    QTable(uint32_t maxPaths = 3);

    /// \param mp maximum number of routes per destination
    void SetMaxPaths(uint32_t mp);
    /// \return maximum number of routes per destination
    uint32_t GetMaxPaths() const;
    /**
     * \param alpha0 learning rate
     * \param gamma discount factor
     * \param epsilon0 initial exploration rate
     */
    void SetLearningParameters(double alpha0, double gamma, double epsilon0);
    /**
     * \param w1 weight of the MAC acknowledgement
     * \param w2 weight of the one-hop delay term
     * \param w3 weight of the residual energy
     */
    void SetRewardWeights(double w1, double w2, double w3 = 0.0);
    /**
     * \param adaptEpsilon adaptive exploration rate
     * \param adaptAlpha adaptive learning rate
     * \param adaptReward energy-dependent reward weights
     */
    void SetAdaptiveFlags(bool adaptEpsilon, bool adaptAlpha, bool adaptReward);

    /// \param on true: probabilistic selection (PMAODV); false: epsilon-greedy
    void SetProbabilistic(bool on)
    {
        m_probabilistic = on;
    }

    /// Route error: raise the exploration rate (adaptive exploration only).
    void OnRouteError();
    /// Periodic decay of the exploration rate.
    void PeriodicEpsilonDecay();
    /// Count a destination sequence number increase.
    void RecordSeqNoUpdate();
    /// Recompute the learning rate from DeltaSeq (adaptive learning rate only).
    void RecomputeAdaptiveAlpha();
    /// \param energyFraction residual energy fraction of this node
    void RecomputeAdaptiveRewardWeights(double energyFraction);
    /// \param frac residual energy fraction of the low-energy weights
    void SetLowEnergyThreshold(double frac);
    /// \param lambda sensitivity of the adaptive learning rate
    void SetSensitivityLambda(double lambda);
    /// \param window window of DeltaSeq
    void SetSeqNoWindow(Time window);

    /// \return learning rate
    double GetAlpha() const
    {
        return m_alpha;
    }

    /// \return exploration rate
    double GetEpsilon() const
    {
        return m_epsilon;
    }

    /// \return number of sequence number increases in the window
    uint32_t GetDeltaSeq() const;

    /**
     * Add an alternate route (replaces the longest one when full).
     * \param rt route
     * \return true if added
     */
    bool AddRoute(const RoutingTableEntry& rt);
    /**
     * Add a route regardless of MaxPaths, or refresh it.
     * \param rt route
     * \return true if added
     */
    bool EnsureRecord(const RoutingTableEntry& rt);
    /**
     * Initial Q-values (Eq. 1) of the routes that were never updated.
     * \param dst destination
     */
    void ReinitQValues(Ipv4Address dst);
    /**
     * \param dst destination
     * \param routes valid routes (appended)
     * \param mainTable routing table used to check the next hops
     * \return number of routes appended
     */
    uint32_t GetRoutes(Ipv4Address dst,
                       std::vector<RoutingTableEntry>& routes,
                       const RoutingTable* mainTable = nullptr) const;
    /**
     * Select the next hop among the primary route and the stored alternates.
     * \param primary primary route
     * \param out selected route
     * \param mainTable routing table used to check the next hops
     * \param exclude previous hop of the packet (never selected)
     * \return false if no candidate was available
     */
    bool SelectEpsilonGreedy(const RoutingTableEntry& primary,
                             RoutingTableEntry& out,
                             const RoutingTable* mainTable = nullptr,
                             Ipv4Address exclude = Ipv4Address());
    /**
     * Q-update of (dst, nextHop).
     * \param dst destination
     * \param nextHop next hop
     * \param ackSuccess 1 if acknowledged, 0 otherwise
     * \param delaySec one-hop delay (s)
     * \param energyFraction residual energy used in the reward
     */
    void UpdateQValue(Ipv4Address dst,
                      Ipv4Address nextHop,
                      double ackSuccess,
                      double delaySec,
                      double energyFraction = 1.0);

    /// \return number of stored routes
    uint32_t Size() const;
    /**
     * \param dst destination
     * \return number of stored routes to dst
     */
    uint32_t CountFor(Ipv4Address dst) const;
    /**
     * \param dst destination
     * \return true if MaxPaths routes to dst are stored
     */
    bool IsFull(Ipv4Address dst) const;
    /// Remove all routes.
    void Clear();
    /// \param os output stream
    void Print(std::ostream& os) const;

  private:
    /**
     * \param vec routes
     * \return route with the largest hop count
     */
    std::vector<QRecord>::iterator FindWorst(std::vector<QRecord>& vec);
    /**
     * \param primary primary route
     * \param mainTable routing table
     * \return primary route followed by the valid alternates
     */
    std::vector<QRecord> BuildCandidates(const RoutingTableEntry& primary,
                                         const RoutingTable* mainTable) const;
    /**
     * \param ackSuccess 1 if acknowledged, 0 otherwise
     * \param delaySec one-hop delay (s)
     * \param energyFrac residual energy fraction
     * \return reward
     */
    double ComputeReward(double ackSuccess, double delaySec, double energyFrac) const;
    /// Remove sequence number events older than the window.
    void PurgeSeqNoEvents();

    std::map<Ipv4Address, std::vector<QRecord>> m_records; ///< routes per destination
    uint32_t m_maxPaths;                                   ///< maximum routes per destination
    double m_alpha;                                        ///< learning rate
    double m_gamma;                                        ///< discount factor
    double m_epsilon;                                      ///< exploration rate
    double m_w1;                                           ///< weight of the acknowledgement
    double m_w2;                                           ///< weight of the delay term
    double m_w3;                                           ///< weight of the residual energy
    bool m_lowEnergyMode;                                  ///< low-energy weights in use
    bool m_adaptEpsilon{false};                            ///< adaptive exploration rate
    bool m_adaptAlpha{false};                              ///< adaptive learning rate
    bool m_adaptReward{false};                             ///< energy-dependent weights
    bool m_probabilistic{false};                           ///< PMAODV selection
    double m_epsilonMin;                                   ///< exploration floor (adaptive)
    double m_epsilonMax;                                   ///< exploration ceiling (adaptive)
    double m_epsilonStep;                                  ///< periodic decay
    double m_epsilonBump;                                  ///< increase on a route error
    double m_lambda;                                       ///< sensitivity of alpha
    Time m_seqNoWindow;                                    ///< window of DeltaSeq
    double m_lowEnergyThresh;                              ///< low-energy threshold
    double m_w1Normal;                                     ///< normal weight w1
    double m_w2Normal;                                     ///< normal weight w2
    double m_w3Normal;                                     ///< normal weight w3
    double m_w1Low;                                        ///< low-energy weight w1
    double m_w2Low;                                        ///< low-energy weight w2
    double m_w3Low;                                        ///< low-energy weight w3
    mutable std::deque<Time> m_seqEvents;                  ///< sequence number increase times
    Ptr<UniformRandomVariable> m_uniform;                  ///< selection random variable
};

} // namespace mpaodv
} // namespace ns3

#endif /* MPAODV_QTABLE_H */
