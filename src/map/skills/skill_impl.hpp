// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#pragma once

#include "../battle.hpp"

class SkillImpl {
public:
	explicit SkillImpl(e_skill skill_id);
	virtual ~SkillImpl() = default;

	e_skill getSkillId() const;

	// The base virtuals are defined inline as no-ops (they used to live in
	// skill_impl.cpp). The first non-inline virtual would be the *key
	// function*, emitting SkillImpl's typeinfo/vtable only in that TU — a TU
	// no tool links. map/skill.hpp holds a unique_ptr<const SkillImpl>, so
	// tool TUs need the complete type, and under the ASan+UBSan CI build
	// GCC referenced `typeinfo for SkillImpl` there: csv2yaml failed to link
	// (undefined reference). With no key function the typeinfo/vtable are
	// emitted weak in every TU that needs them; bodies unchanged. Pinned by
	// the CI linux-sanitizers job.
	/**
	 * Effect of the skill - replaces switch statements in skill_castend_nodamage_id
	 */
	virtual void castendNoDamageId(block_list*, block_list*, uint16, t_tick, int32&) const {
		// no-op
	}

	/**
	 * Effect of the skill - replaces switch statements in skill_castend_damage_id
	 */
	virtual void castendDamageId(block_list*, block_list*, uint16, t_tick, int32&) const {
		// no-op
	}

	/**
	 * Effect of the skill - replaces switch statements in skill_castend_pos2
	 */
	virtual void castendPos2(block_list*, int32, int32, uint16, t_tick, int32&) const {
		// no-op
	}

	/**
	 * Calculate skill damage ratio - replaces battle_calc_attack_skill_ratio() switch
	 */
	virtual void calculateSkillRatio(const Damage*, const block_list*, const block_list*, uint16, int32&, int32) const {
		// no-op
	}

	/**
	 * Modify hit rate for this skill - replaces hit rate switch statements
	 */
	virtual void modifyHitRate(int16&, const block_list*, const block_list*, uint16) const {
		// no-op
	}

	/**
	 * Apply additional effects on the target after damage - called from skill_additional_effect
	 */
	virtual void applyAdditionalEffects(block_list*, block_list*, uint16, t_tick, int32, enum damage_lv) const {
		// no-op
	}

	/**
	 * Apply additional effects on the caster after damage - called from skill_counter_additional_effect
	 */
	virtual void applyCounterAdditionalEffects(block_list*, block_list*, uint16, t_tick, int32&) const {
		// no-op
	}

	/**
	 * Allows modifying the damage data right after initialization.
	 */
	virtual void modifyDamageData(Damage&, const block_list&, const block_list&, uint16) const {
		// no-op
	}

	/**
	 * Allows modifying the element type of attack.
	 */
	virtual void modifyElement(const Damage&, const block_list&, const block_list&, uint16, int32&, int32) const {
		// no-op
	}
 
protected:
	e_skill skill_id_;
};

class StatusSkillImpl : public SkillImpl
{
private:
	bool end_if_running;

public:
	StatusSkillImpl(e_skill skillId, bool end_if_running = false);

	virtual void castendNoDamageId(block_list *src, block_list *target, uint16 skill_lv, t_tick tick, int32& flag) const override;
};

class WeaponSkillImpl : public SkillImpl {
public:
	WeaponSkillImpl(e_skill skill_id);

	virtual void castendDamageId(block_list* src, block_list* target, uint16 skill_lv, t_tick tick, int32& flag) const override;
};

class SkillImplRecursiveDamageSplash : public SkillImpl{
public:
	explicit SkillImplRecursiveDamageSplash(e_skill skill_id);

	virtual void castendDamageId(block_list* src, block_list* target, uint16 skill_lv, t_tick tick, int32& flag) const override;

	virtual void castendPos2(block_list* src, int32 x, int32 y, uint16 skill_lv, t_tick tick, int32& flag) const override;

	virtual int16 getSearchSize(block_list* src, uint16 skill_lv) const;

	virtual int16 getSplashSearchSize(block_list* src, uint16 skill_lv) const;

	virtual int32 getSplashTarget(block_list* src) const;

	virtual void splashSearch(block_list* src, block_list* target, uint16 skill_lv, t_tick tick, int32 flag) const;

	virtual int64 splashDamage(block_list* src, block_list* target, uint16 skill_lv, t_tick tick, int32 flag) const;
};
