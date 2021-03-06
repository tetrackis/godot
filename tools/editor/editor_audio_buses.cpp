#include "editor_audio_buses.h"
#include "editor_node.h"
#include "servers/audio_server.h"


void EditorAudioBus::_notification(int p_what) {

	if (p_what==NOTIFICATION_READY) {

		vu_l->set_under_texture(get_icon("BusVuEmpty","EditorIcons"));
		vu_l->set_progress_texture(get_icon("BusVuFull","EditorIcons"));
		vu_r->set_under_texture(get_icon("BusVuEmpty","EditorIcons"));
		vu_r->set_progress_texture(get_icon("BusVuFull","EditorIcons"));
		scale->set_texture( get_icon("BusVuDb","EditorIcons"));

		disabled_vu = get_icon("BusVuFrozen","EditorIcons");

		prev_active=true;
		update_bus();
		set_process(true);
	}

	if (p_what==NOTIFICATION_DRAW) {

		if (has_focus()) {
			draw_style_box(get_stylebox("focus","Button"),Rect2(Vector2(),get_size()));
		}
	}

	if (p_what==NOTIFICATION_PROCESS) {

		float real_peak[2]={-100,-100};
		bool activity_found=false;

		int cc;
		switch(AudioServer::get_singleton()->get_speaker_mode()) {
			case AudioServer::SPEAKER_MODE_STEREO: cc = 1; break;
			case AudioServer::SPEAKER_SURROUND_51: cc = 4; break;
			case AudioServer::SPEAKER_SURROUND_71: cc = 5; break;
		}

		for(int i=0;i<cc;i++) {
			if (AudioServer::get_singleton()->is_bus_channel_active(get_index(),i)) {
				activity_found=true;
				real_peak[0]=MAX(real_peak[0],AudioServer::get_singleton()->get_bus_peak_volume_left_db(get_index(),i));
				real_peak[1]=MAX(real_peak[1],AudioServer::get_singleton()->get_bus_peak_volume_right_db(get_index(),i));
			}
		}


		if (real_peak[0]>peak_l) {
			peak_l = real_peak[0];
		} else {
			peak_l-=get_process_delta_time()*60.0;
		}

		if (real_peak[1]>peak_r) {
			peak_r = real_peak[1];
		} else {
			peak_r-=get_process_delta_time()*60.0;

		}

		vu_l->set_value(peak_l);
		vu_r->set_value(peak_r);

		if (activity_found!=prev_active) {
			if (activity_found) {
				vu_l->set_over_texture(Ref<Texture>());
				vu_r->set_over_texture(Ref<Texture>());
			} else {
				vu_l->set_over_texture(disabled_vu);
				vu_r->set_over_texture(disabled_vu);

			}

			prev_active=activity_found;
		}

	}

	if (p_what==NOTIFICATION_VISIBILITY_CHANGED) {

		peak_l=-100;
		peak_r=-100;
		prev_active=true;

		set_process(is_visible_in_tree());
	}

}

void EditorAudioBus::update_send() {

	send->clear();
	if (get_index()==0) {
		send->set_disabled(true);
		send->set_text("Speakers");
	} else {
		send->set_disabled(false);
		StringName current_send = AudioServer::get_singleton()->get_bus_send(get_index());
		int current_send_index=0; //by default to master

		for(int i=0;i<get_index();i++) {
			StringName send_name = AudioServer::get_singleton()->get_bus_name(i);
			send->add_item(send_name);
			if (send_name==current_send) {
				current_send_index=i;
			}
		}

		send->select(current_send_index);
	}
}

void EditorAudioBus::update_bus() {

	if (updating_bus)
		return;

	updating_bus=true;

	int index = get_index();

	slider->set_value(AudioServer::get_singleton()->get_bus_volume_db(index));
	track_name->set_text(AudioServer::get_singleton()->get_bus_name(index));
	if (get_index()==0)
		track_name->set_editable(false);

	solo->set_pressed( AudioServer::get_singleton()->is_bus_solo(index));
	mute->set_pressed( AudioServer::get_singleton()->is_bus_mute(index));
	bypass->set_pressed( AudioServer::get_singleton()->is_bus_bypassing_effects(index));
	// effects..
	effects->clear();

	TreeItem *root = effects->create_item();
	for(int i=0;i<AudioServer::get_singleton()->get_bus_effect_count(index);i++) {

		Ref<AudioEffect> afx = AudioServer::get_singleton()->get_bus_effect(index,i);

		TreeItem *fx = effects->create_item(root);
		fx->set_cell_mode(0,TreeItem::CELL_MODE_CHECK);
		fx->set_editable(0,true);
		fx->set_checked(0,AudioServer::get_singleton()->is_bus_effect_enabled(index,i));
		fx->set_text(0,afx->get_name());
		fx->set_metadata(0,i);

	}

	TreeItem *add = effects->create_item(root);
	add->set_cell_mode(0,TreeItem::CELL_MODE_CUSTOM);
	add->set_editable(0,true);
	add->set_selectable(0,false);
	add->set_text(0,"Add Effect");

	update_send();

	updating_bus=false;

}


void EditorAudioBus::_name_changed(const String& p_new_name) {

	if (p_new_name==AudioServer::get_singleton()->get_bus_name(get_index()))
		return;

	String attempt=p_new_name;
	int attempts=1;

	while(true) {

		bool name_free=true;
		for(int i=0;i<AudioServer::get_singleton()->get_bus_count();i++) {

			if (AudioServer::get_singleton()->get_bus_name(i)==attempt) {
				name_free=false;
				break;
			}
		}

		if (name_free) {
			break;
		}

		attempts++;
		attempt=p_new_name+" "+itos(attempts);
	}
	updating_bus=true;

	UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();

	StringName current = AudioServer::get_singleton()->get_bus_name(get_index());
	ur->create_action("Rename Audio Bus");
	ur->add_do_method(AudioServer::get_singleton(),"set_bus_name",get_index(),attempt);
	ur->add_undo_method(AudioServer::get_singleton(),"set_bus_name",get_index(),current);

	for(int i=0;i<AudioServer::get_singleton()->get_bus_count();i++) {
		if (AudioServer::get_singleton()->get_bus_send(i)==current) {
			ur->add_do_method(AudioServer::get_singleton(),"set_bus_send",i,attempt);
			ur->add_undo_method(AudioServer::get_singleton(),"set_bus_send",i,current);
		}
	}

	ur->add_do_method(buses,"_update_bus",get_index());
	ur->add_undo_method(buses,"_update_bus",get_index());


	ur->add_do_method(buses,"_update_sends");
	ur->add_undo_method(buses,"_update_sends");
	ur->commit_action();

	updating_bus=false;

}

void EditorAudioBus::_volume_db_changed(float p_db){

	if (updating_bus)
		return;

	updating_bus=true;

	print_line("new volume: "+rtos(p_db));
	UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();
	ur->create_action("Change Audio Bus Volume",UndoRedo::MERGE_ENDS);
	ur->add_do_method(AudioServer::get_singleton(),"set_bus_volume_db",get_index(),p_db);
	ur->add_undo_method(AudioServer::get_singleton(),"set_bus_volume_db",get_index(),AudioServer::get_singleton()->get_bus_volume_db(get_index()));
	ur->add_do_method(buses,"_update_bus",get_index());
	ur->add_undo_method(buses,"_update_bus",get_index());
	ur->commit_action();

	updating_bus=false;

}
void EditorAudioBus::_solo_toggled(){

	updating_bus=true;

	UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();
	ur->create_action("Toggle Audio Bus Solo");
	ur->add_do_method(AudioServer::get_singleton(),"set_bus_solo",get_index(),solo->is_pressed());
	ur->add_undo_method(AudioServer::get_singleton(),"set_bus_solo",get_index(),AudioServer::get_singleton()->is_bus_solo(get_index()));
	ur->add_do_method(buses,"_update_bus",get_index());
	ur->add_undo_method(buses,"_update_bus",get_index());
	ur->commit_action();

	updating_bus=false;

}
void EditorAudioBus::_mute_toggled(){

	updating_bus=true;

	UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();
	ur->create_action("Toggle Audio Bus Mute");
	ur->add_do_method(AudioServer::get_singleton(),"set_bus_mute",get_index(),mute->is_pressed());
	ur->add_undo_method(AudioServer::get_singleton(),"set_bus_mute",get_index(),AudioServer::get_singleton()->is_bus_mute(get_index()));
	ur->add_do_method(buses,"_update_bus",get_index());
	ur->add_undo_method(buses,"_update_bus",get_index());
	ur->commit_action();

	updating_bus=false;

}
void EditorAudioBus::_bypass_toggled(){

	updating_bus=true;

	UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();
	ur->create_action("Toggle Audio Bus Bypass Effects");
	ur->add_do_method(AudioServer::get_singleton(),"set_bus_bypass_effects",get_index(),bypass->is_pressed());
	ur->add_undo_method(AudioServer::get_singleton(),"set_bus_bypass_effects",get_index(),AudioServer::get_singleton()->is_bus_bypassing_effects(get_index()));
	ur->add_do_method(buses,"_update_bus",get_index());
	ur->add_undo_method(buses,"_update_bus",get_index());
	ur->commit_action();

	updating_bus=false;


}

void EditorAudioBus::_send_selected(int p_which) {

	updating_bus=true;

	UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();
	ur->create_action("Select Audio Bus Send");
	ur->add_do_method(AudioServer::get_singleton(),"set_bus_send",get_index(),send->get_item_text(p_which));
	ur->add_undo_method(AudioServer::get_singleton(),"set_bus_send",get_index(),AudioServer::get_singleton()->get_bus_send(get_index()));
	ur->add_do_method(buses,"_update_bus",get_index());
	ur->add_undo_method(buses,"_update_bus",get_index());
	ur->commit_action();

	updating_bus=false;
}

void EditorAudioBus::_effect_selected() {

	TreeItem *effect = effects->get_selected();
	if (!effect)
		return;
	updating_bus=true;

	if (effect->get_metadata(0)!=Variant()) {

		int index = effect->get_metadata(0);
		Ref<AudioEffect> effect = AudioServer::get_singleton()->get_bus_effect(get_index(),index);
		if (effect.is_valid()) {
			EditorNode::get_singleton()->push_item(effect.ptr());
		}
	}

	updating_bus=false;

}

void EditorAudioBus::_effect_edited() {

	if (updating_bus)
		return;

	TreeItem *effect = effects->get_edited();
	if (!effect)
		return;

	if (effect->get_metadata(0)==Variant()) {
		Rect2 area = effects->get_item_rect(effect);

		effect_options->set_pos(effects->get_global_pos()+area.pos+Vector2(0,area.size.y));
		effect_options->popup();
		//add effect
	} else  {
		int index = effect->get_metadata(0);
		updating_bus=true;

		UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();
		ur->create_action("Select Audio Bus Send");
		ur->add_do_method(AudioServer::get_singleton(),"set_bus_effect_enabled",get_index(),index,effect->is_checked(0));
		ur->add_undo_method(AudioServer::get_singleton(),"set_bus_effect_enabled",get_index(),index,AudioServer::get_singleton()->is_bus_effect_enabled(get_index(),index));
		ur->add_do_method(buses,"_update_bus",get_index());
		ur->add_undo_method(buses,"_update_bus",get_index());
		ur->commit_action();

		updating_bus=false;

	}

}

void EditorAudioBus::_effect_add(int p_which) {

	if (updating_bus)
		return;

	StringName name = effect_options->get_item_metadata(p_which);

	Object *fx = ClassDB::instance(name);
	ERR_FAIL_COND(!fx);
	AudioEffect *afx = fx->cast_to<AudioEffect>();
	ERR_FAIL_COND(!afx);
	Ref<AudioEffect> afxr = Ref<AudioEffect>(afx);

	afxr->set_name(effect_options->get_item_text(p_which));

	UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();
	ur->create_action("Add Audio Bus Effect");
	ur->add_do_method(AudioServer::get_singleton(),"add_bus_effect",get_index(),afxr,-1);
	ur->add_undo_method(AudioServer::get_singleton(),"remove_bus_effect",get_index(),AudioServer::get_singleton()->get_bus_effect_count(get_index()));
	ur->add_do_method(buses,"_update_bus",get_index());
	ur->add_undo_method(buses,"_update_bus",get_index());
	ur->commit_action();
}

void EditorAudioBus::_bind_methods() {

	ClassDB::bind_method("update_bus",&EditorAudioBus::update_bus);
	ClassDB::bind_method("update_send",&EditorAudioBus::update_send);
	ClassDB::bind_method("_name_changed",&EditorAudioBus::_name_changed);
	ClassDB::bind_method("_volume_db_changed",&EditorAudioBus::_volume_db_changed);
	ClassDB::bind_method("_solo_toggled",&EditorAudioBus::_solo_toggled);
	ClassDB::bind_method("_mute_toggled",&EditorAudioBus::_mute_toggled);
	ClassDB::bind_method("_bypass_toggled",&EditorAudioBus::_bypass_toggled);
	ClassDB::bind_method("_name_focus_exit",&EditorAudioBus::_name_focus_exit);
	ClassDB::bind_method("_send_selected",&EditorAudioBus::_send_selected);
	ClassDB::bind_method("_effect_edited",&EditorAudioBus::_effect_edited);
	ClassDB::bind_method("_effect_selected",&EditorAudioBus::_effect_selected);
	ClassDB::bind_method("_effect_add",&EditorAudioBus::_effect_add);
}

EditorAudioBus::EditorAudioBus(EditorAudioBuses *p_buses) {

	buses=p_buses;
	updating_bus=false;

	VBoxContainer *vb = memnew( VBoxContainer );
	add_child(vb);

	set_v_size_flags(SIZE_EXPAND_FILL);

	track_name = memnew( LineEdit );
	vb->add_child(track_name);
	track_name->connect("text_entered",this,"_name_changed");
	track_name->connect("focus_exited",this,"_name_focus_exit");

	HBoxContainer *hbc = memnew( HBoxContainer);
	vb->add_child(hbc);
	hbc->add_spacer();
	solo = memnew( ToolButton );
	solo->set_text("S");
	solo->set_toggle_mode(true);
	solo->set_modulate(Color(0.8,1.2,0.8));
	solo->set_focus_mode(FOCUS_NONE);
	solo->connect("pressed",this,"_solo_toggled");
	hbc->add_child(solo);
	mute = memnew( ToolButton );
	mute->set_text("M");
	mute->set_toggle_mode(true);
	mute->set_modulate(Color(1.2,0.8,0.8));
	mute->set_focus_mode(FOCUS_NONE);
	mute->connect("pressed",this,"_mute_toggled");
	hbc->add_child(mute);
	bypass = memnew( ToolButton );
	bypass->set_text("B");
	bypass->set_toggle_mode(true);
	bypass->set_modulate(Color(1.1,1.1,0.8));
	bypass->set_focus_mode(FOCUS_NONE);
	bypass->connect("pressed",this,"_bypass_toggled");
	hbc->add_child(bypass);
	hbc->add_spacer();

	HBoxContainer *hb = memnew( HBoxContainer );
	vb->add_child(hb);
	slider = memnew( VSlider );
	slider->set_min(-80);
	slider->set_max(24);
	slider->set_step(0.1);

	slider->connect("value_changed",this,"_volume_db_changed");
	hb->add_child(slider);
	vu_l = memnew( TextureProgress );
	vu_l->set_fill_mode(TextureProgress::FILL_BOTTOM_TO_TOP);
	hb->add_child(vu_l);
	vu_l->set_min(-80);
	vu_l->set_max(24);
	vu_l->set_step(0.1);

	vu_r = memnew( TextureProgress );
	vu_r->set_fill_mode(TextureProgress::FILL_BOTTOM_TO_TOP);
	hb->add_child(vu_r);
	vu_r->set_min(-80);
	vu_r->set_max(24);
	vu_r->set_step(0.1);

	scale = memnew( TextureRect );
	hb->add_child(scale);

	add_child(hb);

	effects = memnew( Tree );
	effects->set_hide_root(true);
	effects->set_custom_minimum_size(Size2(0,90)*EDSCALE);
	effects->set_hide_folding(true);
	vb->add_child(effects);
	effects->connect("item_edited",this,"_effect_edited");
	effects->connect("cell_selected",this,"_effect_selected");
	effects->set_edit_checkbox_cell_only_when_checkbox_is_pressed(true);


	send = memnew( OptionButton );
	send->set_clip_text(true);
	send->connect("item_selected",this,"_send_selected");
	vb->add_child(send);

	set_focus_mode(FOCUS_CLICK);

	effect_options = memnew( PopupMenu );
	effect_options->connect("index_pressed",this,"_effect_add");
	add_child(effect_options);
	List<StringName> effects;
	ClassDB::get_inheriters_from_class("AudioEffect",&effects);
	effects.sort_custom<StringName::AlphCompare>();
	for (List<StringName>::Element *E=effects.front();E;E=E->next()) {
		if (!ClassDB::can_instance(E->get()))
			continue;

		Ref<Texture> icon;
		if (has_icon(E->get(),"EditorIcons")) {
			icon = get_icon(E->get(),"EditorIcons");
		}
		String name = E->get().operator String().replace("AudioEffect","");
		effect_options->add_item(name);
		effect_options->set_item_metadata(effect_options->get_item_count()-1,E->get());
		effect_options->set_item_icon(effect_options->get_item_count()-1,icon);
	}


}


void EditorAudioBuses::_update_buses() {

	while(bus_hb->get_child_count()>0) {
		memdelete(bus_hb->get_child(0));
	}

	for(int i=0;i<AudioServer::get_singleton()->get_bus_count();i++) {

		EditorAudioBus *audio_bus = memnew( EditorAudioBus(this) );
		if (i==0) {
			audio_bus->set_self_modulate(Color(1,0.9,0.9));
		}
		bus_hb->add_child(audio_bus);

	}
}

void EditorAudioBuses::register_editor() {

	EditorAudioBuses * audio_buses = memnew( EditorAudioBuses );
	EditorNode::get_singleton()->add_bottom_panel_item("Audio",audio_buses);
}

void EditorAudioBuses::_notification(int p_what) {

	if (p_what==NOTIFICATION_READY) {
		_update_buses();
	}
}


void EditorAudioBuses::_add_bus() {

	UndoRedo *ur = EditorNode::get_singleton()->get_undo_redo();

	//need to simulate new name, so we can undi :(
	ur->create_action("Add Audio Bus");
	ur->add_do_method(AudioServer::get_singleton(),"set_bus_count",AudioServer::get_singleton()->get_bus_count()+1);
	ur->add_undo_method(AudioServer::get_singleton(),"set_bus_count",AudioServer::get_singleton()->get_bus_count());
	ur->add_do_method(this,"_update_buses");
	ur->add_undo_method(this,"_update_buses");
	ur->commit_action();

}

void EditorAudioBuses::_update_bus(int p_index) {

	if (p_index>=bus_hb->get_child_count())
		return;

	bus_hb->get_child(p_index)->call("update_bus");
}

void EditorAudioBuses::_update_sends() {

	for(int i=0;i<bus_hb->get_child_count();i++) {
		bus_hb->get_child(i)->call("update_send");
	}
}

void EditorAudioBuses::_bind_methods() {

	ClassDB::bind_method("_add_bus",&EditorAudioBuses::_add_bus);
	ClassDB::bind_method("_update_buses",&EditorAudioBuses::_update_buses);
	ClassDB::bind_method("_update_bus",&EditorAudioBuses::_update_bus);
	ClassDB::bind_method("_update_sends",&EditorAudioBuses::_update_sends);
}

EditorAudioBuses::EditorAudioBuses()
{

	top_hb = memnew( HBoxContainer );
	add_child(top_hb);

	add = memnew( Button );
	top_hb->add_child(add);;
	add->set_text(TTR("Add"));

	add->connect("pressed",this,"_add_bus");

	Ref<ButtonGroup> bg;
	bg.instance();

	buses = memnew( ToolButton );
	top_hb->add_child(buses);
	buses->set_text(TTR("Buses"));
	buses->set_button_group(bg);
	buses->set_toggle_mode(true);
	buses->set_pressed(true);

	groups = memnew( ToolButton );
	top_hb->add_child(groups);
	groups->set_text(TTR("Groups"));
	groups->set_button_group(bg);
	groups->set_toggle_mode(true);

	bus_scroll = memnew( ScrollContainer );
	bus_scroll->set_v_size_flags(SIZE_EXPAND_FILL);
	bus_scroll->set_enable_h_scroll(true);
	bus_scroll->set_enable_v_scroll(false);
	add_child(bus_scroll);
	bus_hb = memnew( HBoxContainer );
	bus_scroll->add_child(bus_hb);

	group_scroll = memnew( ScrollContainer );
	group_scroll->set_v_size_flags(SIZE_EXPAND_FILL);
	group_scroll->set_enable_h_scroll(true);
	group_scroll->set_enable_v_scroll(false);
	add_child(group_scroll);
	group_hb = memnew( HBoxContainer );
	group_scroll->add_child(group_hb);

	group_scroll->hide();


	set_v_size_flags(SIZE_EXPAND_FILL);


}
