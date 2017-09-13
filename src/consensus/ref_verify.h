// Copyright (c) 2017 The Merit Foundation developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MERIT_CONSENSUS_REF_VERIFY_H
#define MERIT_CONSENSUS_REF_VERIFY_H

class Referral;
class ReferralsViewCache;
class CValidationState;

/** Referral validation functions */

/** Context-independent validity checks */
bool CheckReferral(const Referral& tx, const ReferralsViewCache& refView, CValidationState& state);

#endif // MERIT_CONSENSUS_REF_VERIFY_H
