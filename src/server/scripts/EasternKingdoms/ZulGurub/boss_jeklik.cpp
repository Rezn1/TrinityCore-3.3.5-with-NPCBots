/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "zulgurub.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"

enum Says
{
    SAY_AGGRO               = 0,
    SAY_RAIN_FIRE           = 1,
    SAY_DEATH               = 2
};

enum Spells
{
    SPELL_CHARGE            = 22911,
    SPELL_SONICBURST        = 23918,
    SPELL_SCREECH           = 6605,
    SPELL_SHADOW_WORD_PAIN  = 23952,
    SPELL_MIND_FLAY         = 23953,
    SPELL_CHAIN_MIND_FLAY   = 26044, // Right ID unknown. So disabled
    SPELL_GREATERHEAL       = 23954,
    SPELL_BAT_FORM          = 23966,

    // Batriders Spell
    SPELL_BOMB              = 40332 // Wrong ID but Magmadars bomb is not working...
};

enum BatIds
{
    NPC_BLOODSEEKER_BAT     = 11368,
    NPC_FRENZIED_BAT        = 14965
};

enum Events
{
    EVENT_CHARGE_JEKLIK     = 1,
    EVENT_SONIC_BURST,
    EVENT_SCREECH,
    EVENT_SPAWN_BATS,
    EVENT_SHADOW_WORD_PAIN,
    EVENT_MIND_FLAY,
    EVENT_CHAIN_MIND_FLAY,
    EVENT_GREATER_HEAL,
    EVENT_SPAWN_FLYING_BATS
};

enum Phase
{
    PHASE_ONE               = 1,
    PHASE_TWO               = 2
};

Position const SpawnBat[6] =
{
    { -12291.6220f, -1380.2640f, 144.8304f, 5.483f },
    { -12289.6220f, -1380.2640f, 144.8304f, 5.483f },
    { -12293.6220f, -1380.2640f, 144.8304f, 5.483f },
    { -12291.6220f, -1380.2640f, 144.8304f, 5.483f },
    { -12289.6220f, -1380.2640f, 144.8304f, 5.483f },
    { -12293.6220f, -1380.2640f, 144.8304f, 5.483f }
};

struct boss_jeklik : public BossAI
{
    boss_jeklik(Creature* creature) : BossAI(creature, DATA_JEKLIK) { }

    void Reset() override
    {
        _Reset();
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        Talk(SAY_DEATH);
    }

    void JustEngagedWith(Unit* who) override
    {
        BossAI::JustEngagedWith(who);
        Talk(SAY_AGGRO);
        events.SetPhase(PHASE_ONE);

        events.ScheduleEvent(EVENT_CHARGE_JEKLIK, 20s, 0, PHASE_ONE);
        events.ScheduleEvent(EVENT_SONIC_BURST, 8s, 0, PHASE_ONE);
        events.ScheduleEvent(EVENT_SCREECH, 13s, 0, PHASE_ONE);
        events.ScheduleEvent(EVENT_SPAWN_BATS, 60s, 0, PHASE_ONE);

        me->SetCanFly(true);
        DoCast(me, SPELL_BAT_FORM);
    }

    void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (events.IsInPhase(PHASE_ONE) && !HealthAbovePct(50))
        {
            me->RemoveAurasDueToSpell(SPELL_BAT_FORM);
            me->SetCanFly(false);
            ResetThreatList();
            events.SetPhase(PHASE_TWO);
            events.ScheduleEvent(EVENT_SHADOW_WORD_PAIN, 6s, 0, PHASE_TWO);
            events.ScheduleEvent(EVENT_MIND_FLAY, 11s, 0, PHASE_TWO);
            events.ScheduleEvent(EVENT_CHAIN_MIND_FLAY, 26s, 0, PHASE_TWO);
            events.ScheduleEvent(EVENT_GREATER_HEAL, 50s, 0, PHASE_TWO);
            events.ScheduleEvent(EVENT_SPAWN_FLYING_BATS, 10s, 0, PHASE_TWO);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_CHARGE_JEKLIK:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.f, true))
                    {
                        DoCast(target, SPELL_CHARGE);
                        AttackStart(target);
                    }
                    events.ScheduleEvent(EVENT_CHARGE_JEKLIK, 15s, 30s, 0, PHASE_ONE);
                    break;
                case EVENT_SONIC_BURST:
                    DoCastVictim(SPELL_SONICBURST);
                    events.ScheduleEvent(EVENT_SONIC_BURST, 8s, 13s, 0, PHASE_ONE);
                    break;
                case EVENT_SCREECH:
                    DoCastVictim(SPELL_SCREECH);
                    events.ScheduleEvent(EVENT_SCREECH, 18s, 26s, 0, PHASE_ONE);
                    break;
                case EVENT_SPAWN_BATS:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.f, true))
                        for (uint8 i = 0; i < 6; ++i)
                            if (TempSummon* bat = me->SummonCreature(NPC_BLOODSEEKER_BAT, SpawnBat[i], TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15s))
                                bat->AI()->AttackStart(target);
                    events.ScheduleEvent(EVENT_SPAWN_BATS, 1min, 0, PHASE_ONE);
                    break;
                case EVENT_SHADOW_WORD_PAIN:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.f, true))
                        DoCast(target, SPELL_SHADOW_WORD_PAIN);
                    events.ScheduleEvent(EVENT_SHADOW_WORD_PAIN, 12s, 18s, 0, PHASE_TWO);
                    break;
                case EVENT_MIND_FLAY:
                    DoCastVictim(SPELL_MIND_FLAY);
                    events.ScheduleEvent(EVENT_MIND_FLAY, 16s, 0, PHASE_TWO);
                    break;
                case EVENT_CHAIN_MIND_FLAY:
                    me->InterruptNonMeleeSpells(false);
                    DoCastVictim(SPELL_CHAIN_MIND_FLAY);
                    events.ScheduleEvent(EVENT_CHAIN_MIND_FLAY, 15s, 30s, 0, PHASE_TWO);
                    break;
                case EVENT_GREATER_HEAL:
                    me->InterruptNonMeleeSpells(false);
                    DoCast(me, SPELL_GREATERHEAL);
                    events.ScheduleEvent(EVENT_GREATER_HEAL, 25s, 35s, 0, PHASE_TWO);
                    break;
                case EVENT_SPAWN_FLYING_BATS:
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.f, true))
                        if (TempSummon* flyingBat = me->SummonCreature(NPC_FRENZIED_BAT, target->GetPositionX(), target->GetPositionY(), target->GetPositionZ() + 15.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 15s))
                            flyingBat->AI()->AttackStart(target);
                    events.ScheduleEvent(EVENT_SPAWN_FLYING_BATS, 10s, 15s, 0, PHASE_TWO);
                    break;
                default:
                    break;
            }

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }

        DoMeleeAttackIfReady();
    }
};

// Flying Bat
struct npc_batrider : public ScriptedAI
{
    npc_batrider(Creature* creature) : ScriptedAI(creature)
    {
        Initialize();
    }

    void Initialize()
    {
        _bombTimer = 2000;
    }

    void Reset() override
    {
        Initialize();
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
    }

    void JustEngagedWith(Unit* /*who*/) override { }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        if (_bombTimer <= diff)
        {
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 0.f, true))
            {
                DoCast(target, SPELL_BOMB);
                _bombTimer = 5000;
            }
        }
        else
            _bombTimer -= diff;

        DoMeleeAttackIfReady();
    }

private:
    uint32 _bombTimer;
};

void AddSC_boss_jeklik()
{
    RegisterZulGurubCreatureAI(boss_jeklik);
    RegisterZulGurubCreatureAI(npc_batrider);
}
