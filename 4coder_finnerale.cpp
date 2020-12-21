#define VIM_BINDINGS false

#include "../4coder_default_include.cpp"
#include "4coder_language_ids.cpp"

#if VIM_BINDINGS
#include "4coder_vimmish.cpp"
#endif

#include "../generated/managed_id_metadata.cpp"

#include "4coder_terickson_language.cpp"
#include "../languages/cpp/cpp.cpp"
#include "../languages/odin/odin.cpp"
#include "../languages/rust/rust.cpp"

#include "4coder_fleury_cursor.cpp"


CUSTOM_COMMAND_SIG(goto_compilation_jump)
CUSTOM_DOC("Jump to places from the compilation buffer")
{
    Heap *heap = &global_heap;
    
    View_ID view = get_active_view(app, Access_ReadVisible);
    String_Const_u8 name = string_u8_litexpr("*compilation*");
    Buffer_ID buffer = get_buffer_by_name(app, name, Access_ReadVisible);
    Marker_List *list = language_get_or_make_list_for_buffer(app, heap, buffer);
    
    i64 pos = view_get_cursor_pos(app, view);
    Buffer_Cursor cursor = buffer_compute_cursor(app, buffer, seek_pos(pos));
    
    i32 list_index = get_index_exact_from_list(app, list, cursor.line);
    
    if (list_index >= 0){
        ID_Pos_Jump_Location location = {};
        if (get_jump_from_list(app, list, list_index, &location)){
            if (get_jump_buffer(app, &buffer, &location)){
                jump_to_location(app, view, buffer, location);
            }
        }
    }
    
    // TODO(Leopold): I don't think this should be necessary?
    goto_next_jump(app);
}

CUSTOM_COMMAND_SIG(copy_line)
CUSTOM_DOC("Copy the line the on which the cursor sits.")
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    i64 pos = view_get_cursor_pos(app, view);
    i64 line = get_line_number_from_pos(app, buffer, pos);
    Range_i64 range = get_line_pos_range(app, buffer, line);
    range.start = clamp_bot(range.start - 1, 0);
    i32 size = (i32)buffer_get_size(app, buffer);
    range.end = clamp_top(range.end, size);
    clipboard_post_buffer_range(app, 0, buffer, range);
}

CUSTOM_COMMAND_SIG(half_page_up)
CUSTOM_DOC("Scrolls the view up one view height and moves the cursor up one view height.")
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    f32 page_jump = get_page_jump(app, view) / 2.0;
    move_vertical_pixels(app, -page_jump);
}

CUSTOM_COMMAND_SIG(half_page_down)
CUSTOM_DOC("Scrolls the view down one view height and moves the cursor down one view height.")
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    f32 page_jump = get_page_jump(app, view) / 2.0;
    move_vertical_pixels(app, page_jump);
}


function void
custom_render_buffer(Application_Links *app, View_ID view_id, Face_ID face_id, Buffer_ID buffer, Text_Layout_ID text_layout_id, Rect_f32 rect, Frame_Info frame_info)
{
    ProfileScope(app, "[Custom] render buffer");
    
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = active_view == view_id;
    Rect_f32 prev_clip = draw_set_clip(app, rect);
    
    // NOTE(allen): Cursor shape
    Face_Metrics metrics = get_face_metrics(app, face_id);
    f32 cursor_roundness = (metrics.normal_advance*0.5f)*0.9f;
    f32 mark_thickness = 2.f;
    
    // Language **language = buffer_get_language(app, buffer);
    Token_Array token_array = get_token_array_from_buffer(app, buffer);
    if (token_array.tokens != 0)
    {
        language_paint_tokens(app, buffer, text_layout_id, &token_array);
        if (global_config.use_comment_keyword)
        {
            // NOTE(allen): Scan for TODOs and NOTEs
            if (global_config.use_comment_keyword){
                Comment_Highlight_Pair pairs[] = {
                    {string_u8_litexpr("NOTE"), finalize_color(defcolor_comment_pop, 0)},
                    {string_u8_litexpr("TODO"), finalize_color(defcolor_comment_pop, 1)},
                };
                draw_comment_highlights(app, buffer, text_layout_id,
                                        &token_array, pairs, ArrayCount(pairs));
            }
        }
    }
    else
    {
        Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
        paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));
    }
    
    i64 cursor_pos = view_correct_cursor(app, view_id);
    view_correct_mark(app, view_id);
    
    // NOTE(allen): Scope highlight
    if (global_config.use_scope_highlight){
        Color_Array colors = finalize_color_array(defcolor_back_cycle);
        draw_scope_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }
    
    if (global_config.use_error_highlight || global_config.use_jump_highlight){
        // NOTE(allen): Error highlight
        String_Const_u8 name = string_u8_litexpr("*compilation*");
        Buffer_ID compilation_buffer = get_buffer_by_name(app, name, Access_Always);
        if (global_config.use_error_highlight){
            draw_jump_highlights(app, buffer, text_layout_id, compilation_buffer,
                                 fcolor_id(defcolor_highlight_junk));
        }
        
        // NOTE(allen): Search highlight
        if (global_config.use_jump_highlight){
            Buffer_ID jump_buffer = get_locked_jump_buffer(app);
            if (jump_buffer != compilation_buffer){
                draw_jump_highlights(app, buffer, text_layout_id, jump_buffer,
                                     fcolor_id(defcolor_highlight_white));
            }
        }
    }
    
    // NOTE(allen): Color parens
    if (global_config.use_paren_helper){
        Color_Array colors = finalize_color_array(defcolor_text_cycle);
        draw_paren_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }
    
    // NOTE(allen): Line highlight
    if (global_config.highlight_line_at_cursor && is_active_view){
        i64 line_number = get_line_number_from_pos(app, buffer, cursor_pos);
        draw_line_highlight(app, text_layout_id, line_number,
                            fcolor_id(defcolor_highlight_cursor_line));
    }
    
    // NOTE(allen): Whitespace highlight
    b64 show_whitespace = false;
    view_get_setting(app, view_id, ViewSetting_ShowWhitespace, &show_whitespace);
    if (show_whitespace){
        if (token_array.tokens == 0){
            draw_whitespace_highlight(app, buffer, text_layout_id, cursor_roundness);
        }
        else{
            draw_whitespace_highlight(app, text_layout_id, &token_array, cursor_roundness);
        }
    }
    
#if VIM_BINDINGS
    Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
    
    if (vim_state.search_show_highlight) {
        // NOTE: Vim Search highlight
        i64 pos = visible_range.min;
        while (pos < visible_range.max) {
            Range_i64 highlight_range = vim_search_once_internal(app, view_id, buffer, Scan_Forward, pos, vim_state.last_search_register.string.string, vim_state.search_flags, true);
            if (!range_size(highlight_range)) {
                break;
            }
            vim_draw_character_block_selection(app, buffer, text_layout_id, highlight_range, cursor_roundness, fcolor_id(defcolor_highlight_white));
            pos = highlight_range.max;
        }
    }
    
    // BEGIN: SEEK_HIGHTLIGHTS
#if VIM_USE_CHARACTER_SEEK_HIGHLIGHTS
    if (is_active_view && vim_state.character_seek_show_highlight) {
        // NOTE: Vim Character Seek highlight
        i64 pos = view_get_cursor_pos(app, view_id);
        while (range_contains(visible_range, pos)) {
            i64 seek_pos = vim_character_seek(app, view_id, buffer, pos, SCu8(), vim_state.character_seek_highlight_dir, vim_state.most_recent_character_seek_flags);
            if (seek_pos == pos) {
                break;
            }
            Range_i64 range = Ii64_size(seek_pos, vim_state.most_recent_character_seek.size);
#if VIM_USE_CUSTOM_COLORS
            FColor highlight_color = fcolor_id(defcolor_vim_character_highlight);
#else
            FColor highlight_color = fcolor_id(defcolor_highlight);
#endif
            vim_draw_character_block_selection(app, buffer, text_layout_id, range, cursor_roundness, highlight_color);
            paint_text_color_fcolor(app, text_layout_id, range, fcolor_id(defcolor_at_highlight));
            pos = seek_pos;
        }
    }
#endif
    // END: SEEK_HIGHTLIGHTS
    
    vim_draw_cursor(app, view_id, is_active_view, buffer, text_layout_id, cursor_roundness, mark_thickness, vim_state.mode);
#else
    F4_RenderCursor(app, view_id, is_active_view, buffer, text_layout_id, cursor_roundness, mark_thickness, frame_info);
#endif
    
    // NOTE(allen): Fade ranges
    paint_fade_ranges(app, text_layout_id, buffer);
    
    // NOTE(allen): put the actual text on the actual screen
    draw_text_layout_default(app, text_layout_id);
    
    draw_set_clip(app, prev_clip);
}

BUFFER_HOOK_SIG(custom_begin_buffer)
{
#if VIM_BINDINGS
    vim_begin_buffer(app, buffer_id);
#endif
    language_begin_buffer(app, buffer_id);
    
    return 0;
}

BUFFER_EDIT_RANGE_SIG(custom_buffer_edit_range)
{
#if VIM_BINDINGS
    vim_default_buffer_edit_range(app, buffer_id, new_range, old_cursor_range);
#endif
    language_buffer_edit_range(app, buffer_id, new_range, old_cursor_range);
    
    return 0;
}

function void
custom_render_caller(Application_Links *app, Frame_Info frame_info, View_ID view_id)
{
    ProfileScope(app, "[Custom] render caller");
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);
    
    Rect_f32 region = draw_background_and_margin(app, view_id, is_active_view);
    Rect_f32 prev_clip = draw_set_clip(app, region);
    
    Buffer_ID buffer = view_get_buffer(app, view_id, Access_Always);
    Face_ID face_id = get_face_id(app, buffer);
    Face_Metrics face_metrics = get_face_metrics(app, face_id);
    f32 line_height = face_metrics.line_height;
    f32 digit_advance = face_metrics.decimal_digit_advance;
    
    // NOTE(allen): file bar
    b64 showing_file_bar = false;
    if (view_get_setting(app, view_id, ViewSetting_ShowFileBar, &showing_file_bar) && showing_file_bar){
        Rect_f32_Pair pair = layout_file_bar_on_top(region, line_height);
        draw_file_bar(app, view_id, buffer, face_id, pair.min);
        region = pair.max;
    }
    
    Buffer_Scroll scroll = view_get_buffer_scroll(app, view_id);
    
    Buffer_Point_Delta_Result delta = delta_apply(app, view_id,
                                                  frame_info.animation_dt, scroll);
    if (!block_match_struct(&scroll.position, &delta.point)){
        block_copy_struct(&scroll.position, &delta.point);
        view_set_buffer_scroll(app, view_id, scroll, SetBufferScroll_NoCursorChange);
    }
    if (delta.still_animating){
        animate_in_n_milliseconds(app, 0);
    }
    
    // NOTE(allen): query bars
    region = default_draw_query_bars(app, region, view_id, face_id);
    
    // NOTE(allen): FPS hud
    if (show_fps_hud){
        Rect_f32_Pair pair = layout_fps_hud_on_bottom(region, line_height);
        draw_fps_hud(app, frame_info, face_id, pair.max);
        region = pair.min;
        animate_in_n_milliseconds(app, 1000);
    }
    
    // NOTE(allen): layout line numbers
    Rect_f32 line_number_rect = {};
    if (global_config.show_line_number_margins){
        Rect_f32_Pair pair = layout_line_number_margin(app, buffer, region, digit_advance);
        line_number_rect = pair.min;
        region = pair.max;
    }
    
    // NOTE(allen): begin buffer render
    Buffer_Point buffer_point = scroll.position;
    Text_Layout_ID text_layout_id = text_layout_create(app, buffer, region, buffer_point);
    
    // NOTE(allen): draw line numbers
    if (global_config.show_line_number_margins){
        draw_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);
    }
    
    // NOTE(allen): draw the buffer
    // NOTE(Leopold): but in my way
    custom_render_buffer(app, view_id, face_id, buffer, text_layout_id, region, frame_info);
    
    text_layout_free(app, text_layout_id);
    draw_set_clip(app, prev_clip);
}

function void
custom_tick(Application_Links *app, Frame_Info frame_info)
{
#if VIM_BINDINGS
    vim_tick(app, frame_info);
#endif
    language_tick(app, frame_info);
}

void
custom_layer_init(Application_Links *app)
{
    Thread_Context* tctx = get_thread_context(app);
    
    default_framework_init(app);
    
    init_ext_language();
    
    // TODO(Leopold): Rust must be the first, otherwise cpp will consume the error message lines incorrectly.
    init_language_rust();
    init_language_cpp();
    init_language_odin();
    
    finalize_languages(app);
    
    set_all_default_hooks(app);
    set_language_hooks(app);
    mapping_init(tctx, &framework_mapping);
    setup_default_mapping(&framework_mapping, mapid_global, mapid_file, mapid_code);
    
#if VIM_BINDINGS
    Vim_Key vim_leader = vim_key(KeyCode_Space);
    vim_init(app);
    vim_set_default_hooks(app);
    vim_setup_default_mapping(app, &framework_mapping, vim_leader);
    
    {
        VimMappingScope();
        
        VimSelectMap(vim_map_normal);
        VimBind(windmove_panel_left, vim_key(KeyCode_H, KeyCode_Alt));
        VimBind(windmove_panel_right, vim_key(KeyCode_L, KeyCode_Alt));
        VimBind(w, vim_leader, vim_key(KeyCode_F), vim_key(KeyCode_S));
        
        VimBind(vim_motion_line_start_textual, vim_key(KeyCode_H, KeyCode_Shift));
        VimBind(vim_motion_line_end_textual, vim_key(KeyCode_L, KeyCode_Shift));
        
        VimBind(vim_half_page_up, vim_key(KeyCode_Up));
        VimBind(vim_half_page_down, vim_key(KeyCode_Down));
    }
#else
    {
        MappingScope();
        SelectMapping(&framework_mapping);
        
        SelectMap(mapid_global);
        Bind(load_project, KeyCode_P, KeyCode_Alt);
        
        SelectMap(mapid_file);
        Bind(copy_line, KeyCode_C, KeyCode_Control, KeyCode_Shift);
        Bind(half_page_up, KeyCode_PageUp);
        Bind(half_page_down, KeyCode_PageDown);
        
        SelectMap(mapid_code);
        Bind(goto_compilation_jump, KeyCode_G, KeyCode_Alt);
    }
#endif
    
    set_custom_hook(app, HookID_BeginBuffer, custom_begin_buffer);
    set_custom_hook(app, HookID_BufferEditRange, custom_buffer_edit_range);
    set_custom_hook(app, HookID_RenderCaller, custom_render_caller);
    set_custom_hook(app, HookID_Tick, custom_tick);
}
