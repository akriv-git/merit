// Copyright (c) 2017-2018 The Merit Foundation developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pog/reward.h"

#include <algorithm>
#include <numeric>

namespace pog
{
    AmbassadorLottery RewardAmbassadors(
            int height,
            const referral::AddressANVs& winners,
            CAmount total_reward)
    {
        /**
         * Increase ANV precision on block 16000
         */
        CAmount fixed_precision = height < 16000 ? 100 : 1000;

        CAmount total_anv =
            std::accumulate(std::begin(winners), std::end(winners), CAmount{0},
                    [](CAmount acc, const referral::AddressANV& v)
                    {
                        return acc + v.anv;
                    });

        Rewards rewards(winners.size());
        std::transform(std::begin(winners), std::end(winners), std::begin(rewards),
                [total_reward, total_anv, fixed_precision](const referral::AddressANV& v)
                {
                    double percent = (v.anv*fixed_precision) / total_anv;
                    CAmount reward = (total_reward * percent) / fixed_precision;
                    assert(reward <= total_reward);
                    return AmbassadorReward{v.address_type, v.address, reward};
                });

        Rewards filtered_rewards;
        filtered_rewards.reserve(rewards.size());
        std::copy_if(std::begin(rewards), std::end(rewards),
                std::back_inserter(filtered_rewards),
                [](const AmbassadorReward& reward) {
                    return reward.amount > 0;
                });

        CAmount total_rewarded =
            std::accumulate(std::begin(filtered_rewards), std::end(filtered_rewards), CAmount{0},
                    [](CAmount acc, const AmbassadorReward& reward)
                    {
                        return acc + reward.amount;
                    });

        assert(total_rewarded >= 0);
        assert(total_rewarded <= total_reward);

        auto remainder = total_reward - total_rewarded;

        assert(remainder >= 0);
        assert(remainder <= total_reward);

        return {filtered_rewards, remainder};
    }

    int OldComputeTotalInviteLotteryWinners(
            int height,
            const InviteLotteryParams& lottery,
            const Consensus::Params& params)
    {
        LogPrint(BCLog::VALIDATION, "Invites used: %d created: %d period: %d used per block: %d\n",
                lottery.invites_used,
                lottery.invites_created,
                params.daedalus_block_window,
                lottery.mean_used);

        const auto period = (height - params.vDeployments[Consensus::DEPLOYMENT_DAEDALUS].start_block) /
            params.daedalus_block_window;

        /**
         * Distribute out invites at the maximum rate for the very first period
         * to kickstart daedalus.
         */
        if(period < 1) {
            return params.daedalus_max_invites_per_block;
        }

        assert(lottery.invites_used >= 0);
        assert(params.daedalus_min_one_invite_for_every_x_blocks > 0);
        assert(params.daedalus_min_one_invite_for_every_x_blocks <= params.daedalus_block_window);

        /**
         * If no invites are generated that means that the amount used fell
         * under 1 per block during that period. Therefore replace at least
         * the invites used during the period in this block plus at least 1
         * ever ten minutes of that period under the assumption that some invites
         * will leak to users who forget about them or abandon merit. To prevent
         * starvation we need to be able to always generate some merit over the period.
         */
        if(lottery.invites_created == 0) {
            return lottery.invites_used +
                (params.daedalus_block_window / params.daedalus_min_one_invite_for_every_x_blocks);
        }

        const auto invites_used_per_block = lottery.invites_used / params.daedalus_block_window;
        const auto total_winners =
            std::max(0, std::min(invites_used_per_block, params.daedalus_max_invites_per_block));

        assert(total_winners >= 0 && total_winners <= params.daedalus_max_invites_per_block);
        return total_winners;
    }

    int ImpComputeTotalInviteLotteryWinners(
            int height,
            const InviteLotteryParamsVec& lottery_points,
            const Consensus::Params& params)
    {
        assert(lottery_points.size() == 2);

        const auto& block1 = lottery_points[0];
        const auto& block2 = lottery_points[1];

        LogPrint(BCLog::VALIDATION, "Invites used: %d created: %d period: %d used per block: %d\n",
                block1.invites_used,
                block1.invites_created,
                params.daedalus_block_window,
                block1.mean_used);


        int min_total_winners = 0;
        if(block1.invites_created <= (block1.blocks / params.imp_miner_reward_for_every_x_blocks)) {
            min_total_winners = 
                (block1.blocks / params.imp_min_one_invite_for_every_x_blocks);
        }

        const double mean_diff = block1.mean_used - block2.mean_used;

        //Assume we need more or less than what was used before.
        //This allows invites to grow or shrink exponentially.
        const int change = mean_diff >= 0 ?
            std::ceil(mean_diff) : 
            std::floor(mean_diff);

        const int total_winners = std::max(
                min_total_winners,
                static_cast<int>(std::floor(block1.mean_used) + change));

        assert(total_winners >= 0);
        return total_winners;
    }

    double ComputeUsedInviteMean(const InviteLotteryParams& lottery)
    {
        if(lottery.blocks <= 0) {
            return 0.0;
        }

        auto mean = static_cast<double>(lottery.invites_used) /
            static_cast<double>(lottery.blocks);
        return mean;
    }

    int ComputeTotalInviteLotteryWinners(
            int height,
            const InviteLotteryParamsVec& lottery,
            const Consensus::Params& params)
    {
        assert(lottery.size() > 0 && lottery.size() <=2);
        if(height >= params.imp_invites_blockheight) {
            return ImpComputeTotalInviteLotteryWinners(height, lottery, params);
        }
        return OldComputeTotalInviteLotteryWinners(height, lottery[0], params);
    }

    InviteRewards RewardInvites(const referral::ConfirmedAddresses& winners)
    {
        assert(winners.size() >= 0);

        const auto INVITES_PER_WINNER = 1;

        InviteRewards rewards(winners.size());
        std::transform(winners.begin(), winners.end(), rewards.begin(),
                [INVITES_PER_WINNER](const referral::ConfirmedAddress& winner) -> InviteReward {
                    return {
                        winner.address_type,
                        winner.address,
                        INVITES_PER_WINNER
                    };
                });

        assert(rewards.size() == winners.size());
        return rewards;
    }

} // namespace pog
