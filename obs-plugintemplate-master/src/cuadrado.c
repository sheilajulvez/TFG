

//video tick
source_def.video_tick = function(data, dt) local video_info =
	obs.obs_video_info()

		-- Check to see if stream resoluiton resized and
	recreate texture if so-- TODO : Is there a signal that does this
	? if obs.obs_get_video_info(video_info) and
		  (video_info.base_width ~ =
			   data.width or video_info.base_height ~ = data.height)
			  then data.width = video_info.base_width data.height =
		  video_info.base_height create_whiteboard_texture(data)
    end

    if data.texture == nil then
        return
    end
    
    if not data.active then
        return
    end

    if needs_clear then
        local prev_render_target = obs.gs_get_render_target()
        local prev_zstencil_target = obs.gs_get_zstencil_target()

        obs.gs_viewport_push()
        obs.gs_set_viewport(0, 0, data.width, data.height)

        obs.obs_enter_graphics()
        obs.gs_set_render_target(data.texture, nil)
        obs.gs_clear(obs.GS_CLEAR_COLOR, obs.vec4(), 1.0, 0)

        obs.gs_viewport_pop()
        obs.gs_set_render_target(prev_render_target, prev_zstencil_target)

        obs.obs_leave_graphics()

        needs_clear = false
    end

    if swap_color then
        color_index = color_index + 1
        if color_index > color_count then
            color_index = 1
        end
        swap_color = false
    end
    
    -- The hotkeyed size toggle increases size by increments of 2,
    -- starting from 2. This is due to single pixel increments being
    -- generally unnoticeable.
    if toggle_size then
        -- If the current size is an even number, increment by 2,
        -- otherwise increment by 1 to make it even.
        odd = size % 2
        if odd == 1 then
            size = size + 1
        else
            size = size + 2
        end
        if size > size_max then
            size = 2
        end
        update_vertices()
        toggle_size = false
    end
    
    -- If the size of the pencil was changed, we need to update the
    -- vertices used to draw our line.
    if setting_update then
        update_vertices()
        setting_update = false
    end
        
    local mouse_down = winapi.GetAsyncKeyState(winapi.VK_LBUTTON)
    if mouse_down then
        local mouse_pos = winapi.GetCursorPos()
        if data.mouse_pos == nil then
            data.mouse_pos = mouse_pos
        end
        
        local window = winapi.GetForegroundWindow()
        window_name = winapi.InternalGetWindowText(window, nil)
        if window_match(window_name) and 
        (string.find(window_name, window_projector_name, 1, true) or
        string.find(window_name, fullscreen_projector_name, 1, true)) then
            winapi.ScreenToClient(window, mouse_pos)
            local window_rect = winapi.GetClientRect(window)
            
            local output_aspect = data.width / data.height

            local window_width = window_rect.right - window_rect.left
            local window_height = window_rect.bottom - window_rect.top
            local window_aspect = window_width / window_height
            local offset_x = 0
            local offset_y = 0
            if window_aspect >= output_aspect then
                offset_x = (window_width - window_height * output_aspect) / 2
            else
                offset_y = (window_height - window_width / output_aspect) / 2
            end

            mouse_pos.x = data.width * (mouse_pos.x - offset_x) / (window_width - offset_x*2)
            mouse_pos.y = data.height * (mouse_pos.y - offset_y) / (window_height - offset_y*2)
            
            if valid_position(mouse_pos.x, mouse_pos.y, data.mouse_pos.x, data.mouse_pos.y, data.width, data.height) then
                effect = obs.obs_get_base_effect(obs.OBS_EFFECT_DEFAULT)
                if not effect then
                    return
                end

                obs.obs_enter_graphics()

                local prev_render_target = obs.gs_get_render_target()
                local prev_zstencil_target = obs.gs_get_zstencil_target()

                obs.gs_set_render_target(data.texture, nil)
                obs.gs_viewport_push()
                obs.gs_set_viewport(0, 0, data.width, data.height)
                obs.gs_projection_push()
                obs.gs_ortho(0, data.width, 0, data.height, 0.0, 1.0)

                obs.gs_blend_state_push()
                obs.gs_reset_blend_state()
                
                -- Set the color being used (or set the eraser).
                local solid = obs.obs_get_base_effect(obs.OBS_EFFECT_SOLID)
                local color = obs.gs_effect_get_param_by_name(solid, "color")
                local tech  = obs.gs_effect_get_technique(solid, "Solid")

                if eraser then
                    obs.gs_blend_function(obs.GS_BLEND_SRCALPHA, obs.GS_BLEND_SRCALPHA)
                    obs.gs_effect_set_vec4(color, eraser_v4)
                else
                    local color_v4 = obs.vec4()
                    obs.vec4_from_rgba(color_v4, color_array[color_index])
                    obs.gs_effect_set_vec4(color, color_v4)
                end
            
                obs.gs_technique_begin(tech)
                obs.gs_technique_begin_pass(tech, 0)

                -- Calculate distance mouse has traveled since our
                -- last update.
                local dx = mouse_pos.x - data.mouse_pos.x
                local dy = mouse_pos.y - data.mouse_pos.y
                local len = math.sqrt(dx*dx + dy*dy)
                local angle = math.atan2(dy, dx)
                
                -- Perform matrix transformations for the dot at the
                -- start of the line (start cap).
                obs.gs_matrix_push()
                obs.gs_matrix_identity()
                obs.gs_matrix_translate3f(data.mouse_pos.x, data.mouse_pos.y, 2)

                obs.gs_matrix_push()
                obs.gs_matrix_translate3f(-size, -size, 2)
                
                -- Draw start of line.
                obs.gs_load_vertexbuffer(dot_vert)
                obs.gs_draw(obs.GS_LINESTRIP, 0, 0)

                obs.gs_matrix_pop()

                -- Perform matrix transformations for the actual line.
                obs.gs_matrix_rotaa4f(0, 0, 1, angle)
                obs.gs_matrix_translate3f(0, -size, 2)
                obs.gs_matrix_scale3f(len / size, 1.0, 1.0)

                -- Draw actual line.
                obs.gs_load_vertexbuffer(line_vert)
                obs.gs_draw(obs.GS_TRIS, 0, 0)


                -- Perform matrix transforms for the dot at the end
                -- of the line (end cap).
                obs.gs_matrix_identity()
                obs.gs_matrix_translate3f(mouse_pos.x, mouse_pos.y, 2)
                obs.gs_matrix_translate3f(-size, -size, 2)
                obs.gs_load_vertexbuffer(dot_vert)
                obs.gs_draw(obs.GS_LINESTRIP, 0, 0)

                -- Done drawing line, restore everything.
                obs.gs_technique_end_pass(tech)
                obs.gs_technique_end(tech)

                obs.gs_matrix_pop()

                obs.gs_projection_pop()
                obs.gs_viewport_pop()
                obs.gs_blend_state_pop()
                obs.gs_set_render_target(prev_render_target, prev_zstencil_target)

                obs.obs_leave_graphics()
                
            end

            data.mouse_pos = mouse_pos
        end
    else
        data.mouse_pos = nil
    end
end


//video render
source_def.video_render = function(data, effect) effect =
	obs.obs_get_base_effect(obs.OBS_EFFECT_DEFAULT)

		if effect and
	data.texture then obs.gs_blend_state_push()
		obs.gs_reset_blend_state() obs.gs_matrix_push()
			obs.gs_matrix_identity()

				while obs.gs_effect_loop(effect, "Draw") do obs
					.obs_source_draw(data.texture, 0, 0, 0,
							 0, false);
end

	obs.gs_matrix_pop() obs.gs_blend_state_pop() end end

//get width get height

	source_def.get_width = function(data) return 0 end
source_def.get_height =function(data) return 0 end


function update_vertices()
    obs.obs_enter_graphics()
    
    -- LINE VERTICES
    -- Create vertices for line of given width (user-defined 'size').
    -- These vertices are for two triangles that make up each line.
    obs.gs_render_start(true)
    local width = size * 2
    obs.gs_vertex2f(0, 0)
    obs.gs_vertex2f(size, 0)
    obs.gs_vertex2f(0, width)
    obs.gs_vertex2f(0, width)
    obs.gs_vertex2f(size, width)
    obs.gs_vertex2f(size, 0)
    
    line_vert = obs.gs_render_save()
    
    
    -- DOT VERTICES
    -- Create vertices for a dot (filled circle) of specified width,
    -- which is used to round off the ends of the lines.
    local decision = 0
    local xcoord = 0
     -- set initial ycoord to radius of circle (user-defined 'size')
    local ycoord = size
    
    obs.gs_render_start(true)
    while ycoord >= xcoord do
        -- shift the actual coordinates by the size of the circle,
        -- so the entire circle has positive coordinates
        local y_pos = ycoord + size
        local y_neg = -ycoord + size
        local x_pos = xcoord + size
        local x_neg = -xcoord + size
        -- create horizontal lines to fill the entire circle
        obs.gs_vertex2f(x_pos, y_pos)
        obs.gs_vertex2f(x_neg, y_pos)
        obs.gs_vertex2f(x_pos, y_neg)
        obs.gs_vertex2f(x_neg, y_neg)
        obs.gs_vertex2f(y_pos, x_pos)
        obs.gs_vertex2f(y_neg, x_pos)
        obs.gs_vertex2f(y_pos, x_neg)
        obs.gs_vertex2f(y_neg, x_neg)
        
        -- update coordinates and decision parameter
        xcoord = xcoord + 1
        if decision < 0 then
            ycoord = ycoord
            decision = decision + (2 * xcoord) + 1
        else
            ycoord = ycoord - 1
            decision = decision + (2 * xcoord) + 1 - (2 * ycoord)
        end
    end
    
    dot_vert = obs.gs_render_save()    
    obs.obs_leave_graphics()
end
