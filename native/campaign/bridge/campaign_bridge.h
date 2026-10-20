#pragma once
#include "World.h"
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/array.hpp>

// The only Godot-facing campaign owner. No battle methods are exposed.
class CampaignBridge : public godot::RefCounted {
    GDCLASS(CampaignBridge, godot::RefCounted)
    Birthright::Campaign::World world;
    Birthright::Campaign::Id player = 0;
    bool initialized = false;
    std::string operation(const char *kind) const;
protected:
    static void _bind_methods();
public:
    godot::Dictionary create_world(const godot::String &geography, int64_t seed, const godot::String &origin);
    godot::Dictionary advance_days(int64_t days);
    godot::Dictionary summary() const;
    godot::Dictionary geometry() const;
    godot::Array areas() const;
    godot::Dictionary inspect_area(const godot::String &id) const;
    godot::Array people() const;
    godot::Dictionary inspect_person(const godot::String &id) const;
    godot::Array realms() const;
    godot::Array item_catalog() const;
    godot::Dictionary inventory() const;
    godot::Dictionary warband() const;
    godot::Dictionary recruit(const godot::String &culture_id, int64_t count);
    godot::Dictionary dismiss_troop(const godot::String &troop_id);
    godot::Dictionary equip_item(int64_t slot, const godot::String &id);
    godot::Array recent_events(int64_t limit) const;
    godot::String snapshot_text() const;
    godot::Dictionary restore_snapshot(const godot::String &text, const godot::String &player_id);
};
