#pragma once

#include "World.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace Birthright::Campaign::CultureScaffold
{
// BR-068: draft contracts only. Not attached to Snapshot, serialized or ticked.
// All paths use the existing pantheon; no favor/power/health authority lives here.
constexpr std::uint32_t ContractVersion = 1;
using DivineProfile = std::array<double, DeityCount>;

inline bool IsProfile(const DivineProfile& profile)
{
    double sum = 0;
    for (double weight : profile)
    {
        if (!std::isfinite(weight) || weight < 0 || weight > 1) return false;
        sum += weight;
    }
    return std::abs(sum - 1) <= 1e-9;
}

// Authored allegiance weights, not raw EarnedFavor or inherited power.
// Reject atomically; scaling first avoids overflow for large finite weights.
inline Result NormalizeAlignment(const DivineProfile& weights, DivineProfile& output)
{
    double maximum = 0;
    for (double weight : weights)
    {
        if (!std::isfinite(weight) || weight < 0)
            return {false, "culture_alignment_invalid", "BR-068: alignment weights must be finite and nonnegative."};
        maximum = std::max(maximum, weight);
    }
    if (maximum == 0)
        return {false, "culture_alignment_empty", "BR-068: select at least one existing divine path."};
    DivineProfile candidate{};
    double sum = 0;
    for (std::size_t d = 0; d < DeityCount; ++d) sum += weights[d] / maximum;
    for (std::size_t d = 0; d < DeityCount; ++d) candidate[d] = (weights[d] / maximum) / sum;
    output = candidate;
    return {true, "ok", {}};
}

// A diagnostic distance, not a yield, hostility or suppression formula.
// Invalid/uninitialized profiles have no distance, rather than perfect agreement.
inline std::optional<double> DivineDistance(const DivineProfile& first, const DivineProfile& second)
{
    if (!IsProfile(first) || !IsProfile(second)) return std::nullopt;
    double distance = 0;
    for (std::size_t d = 0; d < DeityCount; ++d) distance += std::abs(first[d] - second[d]);
    return std::clamp(distance * 0.5, 0.0, 1.0);
}

struct RulerAlignment
{
    Id CharacterId = 0;
    DivineProfile Allegiance{}; // Unset until authored/migrated; usually one dominant weight.
};
struct DomainCulture
{
    Id DomainId = 0;
    DivineProfile Tradition{}; // Institutional memory; succession does not replace it.
};
struct LocalTradition
{
    Id AreaId = 0, CultureId = 0; // Refers to an existing cohort, never duplicates its population.
    DivineProfile Tradition{}; // Local expression; never edit the shared Culture to convert one area.
};
struct CulturalAccommodation
{
    Id AreaId = 0, CultureId = 0, DomainId = 0;
    double Adaptation = 0; // 0..1 compatibility with this polity without changing cultural identity.
};
struct CandidateState
{
    std::uint32_t Version = ContractVersion;
    std::map<Id, RulerAlignment> Rulers;
    std::map<Id, DomainCulture> Domains;
    std::map<std::pair<Id, Id>, LocalTradition> LocalTraditions; // Area, culture.
    // Accommodation needs the receiving domain identity; reconquest policy is still deferred.
    std::vector<CulturalAccommodation> Accommodation;
};

// Missing values require an explicit implementation/tuning decision; zero is not a default balance.
struct TuningProposal
{
    std::uint32_t Version = ContractVersion;
    std::optional<double> TypicalPrimaryWeight; // 0..1; generated rulers normally heavily skewed.
    std::optional<double> RulerResponseDays, BorderResponseDays, ConversionResponseDays, AdaptationResponseDays;
    std::optional<double> NeighborWeight, SignificantMismatch, MaximumYieldLoss;
    std::optional<double> SuppressionCostPerPersonDay; // Existing currency per resident per campaign day.
};

// Future pure policy inputs: World resolves IDs and validates references before quoting.
struct ResidentCultureContext
{
    Id AreaId = 0, CultureId = 0;
    std::int64_t Population = 0; // Borrowed snapshot value, never independently persisted.
    DivineProfile Tradition{};
    double Adaptation = 0;
};
struct RegionContext
{
    std::uint32_t Version = ContractVersion;
    std::string WorldId;
    std::int64_t Day = 0;
    Id RegionId = 0, DomainId = 0, RulerId = 0;
    DivineProfile RulerAlignment{}, DomainTradition{};
    // Evaluate mismatch for each cohort BEFORE population weighting; a mean hides minorities.
    std::vector<ResidentCultureContext> Residents;
};
struct RegionalEffectQuote
{
    bool Evaluated = false; // An unevaluated quote must never authorize a spend.
    double Mismatch = 0;
    double YieldMultiplier = 1;
    double RequestedSuppressionPerDay = 0;
    std::string Reason;
};
struct DiplomacyContext
{
    std::uint32_t Version = ContractVersion;
    std::string WorldId;
    std::int64_t Day = 0;
    Id ActingDomainId = 0, OtherDomainId = 0;
    DivineProfile ActingRuler{}, OtherRuler{}, ActingTradition{}, OtherTradition{};
};
struct DiplomacyEffectQuote
{
    bool Evaluated = false;
    double AttitudeAdjustment = 0; // Policy-defined units; no war permission or ownership mutation.
    std::string Reason;
};
}
