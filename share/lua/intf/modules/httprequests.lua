--[==========================================================================[
 httprequests.lua: code for processing httprequests commands and output
--[==========================================================================[
 Copyright (C) 2007 the VideoLAN team

 Authors: Antoine Cellerier <dionoea at videolan dot org>
 Rob Jonson <rob at hobbyistsoftware.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
--]==========================================================================]

local httprequests = {}

local common = require ("common")
local dkjson = require ("dkjson")



--Round the number to the specified precision
function round(what, precision)
    if type(what) == "string" then
        what = common.us_tonumber(what)
    end
    if type(what) == "number" then
        return math.floor(what*math.pow(10,precision)+0.5) / math.pow(10,precision)
    end
    return nil
end
httprequests.round = round

--split text where it matches the delimiter
function strsplit(text, delimiter)
    local strfind = string.find
    local strsub = string.sub
    local tinsert = table.insert
    local list = {}
    local pos = 1
    if strfind("", delimiter, 1) then -- this would result in endless loops
        error("delimiter matches empty string!")
    end
    local i=1
    while 1 do
        local first, last = strfind(text, delimiter, pos)
        if first then -- found?
            tinsert(list,i, strsub(text, pos, first-1))
            pos = last+1
        else
            tinsert(list,i, strsub(text, pos))
            break
        end
        i = i+1
    end
    return list
end
httprequests.strsplit = strsplit

--main function to process commands sent with the request

processcommands = function ()

    local input = _GET['input']
    local command = _GET['command']
    local id = tonumber(_GET['id'] or -1)
    local val = _GET['val']
    local options = _GET['option']
    local band = tonumber(_GET['band'])
    local name = _GET['name']
    local duration = tonumber(_GET['duration'])
    if type(options) ~= "table" then -- Deal with the 0 or 1 option case
        options = { options }
    end

    if command == "in_play" then
        --[[
        vlc.msg.err( "<options>" )
        for a,b in ipairs(options) do
        vlc.msg.err(b)
        end
        vlc.msg.err( "</options>" )
        --]]
        vlc.playlist.add({{path=vlc.strings.make_uri(input),options=options,name=name,duration=duration}})
    elseif command == "addsubtitle" then
        vlc.player.add_subtitle(val)
    elseif command == "in_enqueue" then
        vlc.playlist.enqueue({{path=vlc.strings.make_uri(input),options=options,name=name,duration=duration}})
    elseif command == "pl_play" then
        if id == -1 then
            vlc.playlist.play()
        else
            vlc.playlist.gotoitem(id)
        end
    elseif command == "pl_pause" then
        if vlc.playlist.status() == "stopped" then
            if id == -1 then
                vlc.playlist.play()
            else
                vlc.playlist.gotoitem(id)
            end
        else
            vlc.playlist.pause()
        end
    elseif command == "pl_forcepause" then
        if vlc.playlist.status() == "playing" then
            vlc.playlist.pause()
        end
    elseif command == "pl_forceresume" then
        if vlc.playlist.status() == "paused" then
            vlc.playlist.pause()
        end
    elseif command == "pl_stop" then
        vlc.playlist.stop()
    elseif command == "pl_next" then
        vlc.playlist.next()
    elseif command == "pl_previous" then
        vlc.playlist.prev()
    elseif command == "pl_delete" then
        vlc.playlist.delete(id)
    elseif command == "pl_empty" then
        vlc.playlist.clear()
    elseif command == "pl_sort" then
        vlc.playlist.sort( val, id > 0 )
    elseif command == "pl_random" then
        vlc.playlist.random()
    elseif command == "pl_loop" then
        vlc.playlist.loop()
    elseif command == "pl_repeat" then
        vlc.playlist.repeat_()
    elseif command == "fullscreen" then
        if vlc.object.vout() then
            vlc.video.fullscreen()
        end
    elseif command == "snapshot" then
        common.snapshot()
    elseif command == "volume" then
        common.volume(val)
    elseif command == "seek" then
        common.seek(val)
    elseif command == "key" then
        common.hotkey("key-"..val)
    elseif command == "audiodelay" then
        val = common.us_tonumber(val)
        vlc.player.set_audio_delay(val)
    elseif command == "rate" then
        val = common.us_tonumber(val)
        if val >= 0 then
            vlc.player.set_rate(val)
        end
    elseif command == "subdelay" then
        val = common.us_tonumber(val)
        vlc.player.set_subtitle_delay(val)
    elseif command == "aspectratio" then
        if vlc.object.vout() then
            vlc.var.set(vlc.object.vout(),"aspect-ratio",val)
        end
    elseif command == "preamp" then
        val = common.us_tonumber(val)
        vlc.equalizer.preampset(val)
    elseif command == "equalizer" then
        val = common.us_tonumber(val)
        vlc.equalizer.equalizerset(band,val)
    elseif command == "enableeq" then
        if val == '0' then vlc.equalizer.enable(false) else vlc.equalizer.enable(true) end
    elseif command == "setpreset" then
        vlc.equalizer.setpreset(val)
    elseif command == "title" then
        vlc.player.title_goto(val)
    elseif command == "chapter" then
        vlc.player.chapter_goto(val)
    elseif command == "audio_track" then
        vlc.player.toggle_audio_track(val)
    elseif command == "video_track" then
        vlc.player.toggle_video_track(val)
    elseif command == "subtitle_track" then
        vlc.player.toggle_spu_track(val)
    elseif command == "set_renderer" then
        local rd = get_renderer_discovery()
        if not rd then
            return
        end
        rd:select(id)
    elseif command == "unset_renderer" then
        rd:select(-1)
    end

    local input = nil
    local command = nil
    local id = nil
    local val = nil

end
httprequests.processcommands = processcommands

--utilities for formatting output

function xmlString(s)
    if (type(s)=="string") then
        return vlc.strings.convert_xml_special_chars(s)
    elseif (type(s)=="number") then
        return common.us_tostring(s)
    else
        return tostring(s)
    end
end
httprequests.xmlString = xmlString

--dkjson outputs numbered tables as arrays
--so we don't need the array indicators
function removeArrayIndicators(dict)
    local newDict=dict

    for k,v in pairs(dict) do
        if (type(v)=="table") then
            local arrayEntry=v._array
            if arrayEntry then
                v=arrayEntry
            end

            dict[k]=removeArrayIndicators(v)
        end
    end

    return newDict
end
httprequests.removeArrayIndicators = removeArrayIndicators

printTableAsJson = function (dict)
    dict=removeArrayIndicators(dict)

    local output=dkjson.encode (dict, { indent = true })
    print(output)
end
httprequests.printTableAsJson = printTableAsJson

local printXmlKeyValue = function (k,v,indent)
    print("\n")
    for i=1,indent do print(" ") end
    if (k) then
        --XML element naming rules: this is a bit more conservative
        --than it strictly needs to be
        if not string.match(k, "^[a-zA-Z_][a-zA-Z0-9_%-%.]*$")
            or string.match(k, "^[xX][mM][lL]") then
            print('<element name="'..xmlString(k)..'">')
            k = "element"
        else
            print("<"..k..">")
        end
    end

    if (type(v)=="table") then
        printTableAsXml(v,indent+2)
    else
        print(xmlString(v))
    end

    if (k) then
        print("</"..k..">")
    end
end
httprequests.printXmlKeyValue = printXmlKeyValue

printTableAsXml = function (dict,indent)
    for k,v in pairs(dict) do
        printXmlKeyValue(k,v,indent)
    end
end
httprequests.printTableAsXml = printTableAsXml

--[[
function logTable(t,pre)
local pre = pre or ""
for k,v in pairs(t) do
vlc.msg.err(pre..tostring(k).." : "..tostring(v))
if type(v) == "table" then
a(v,pre.."  ")
end
end
end
--]]

--main accessors

getplaylist = function ()
    local p
    if _GET["search"] then
        if _GET["search"] ~= "" then
            _G.search_key = _GET["search"]
        else
            _G.search_key = nil
        end
        local key = vlc.strings.decode_uri(_GET["search"])
        p = vlc.playlist.search(key)
    else
        p = vlc.playlist.list()
    end

    --logTable(p) --Uncomment to debug

    return p
end
httprequests.getplaylist = getplaylist

parseplaylist = function (list)
    local playlist = {}
    local current_item_id = vlc.playlist.current()
    for i, item in ipairs(list) do
        local result={}

        local name, path = item.name or ""
        local path = item.path or ""

        -- Is the item the one currently played
        if(current_item_id ~= nil) then
            if(current_item_id == item.id) then
                result.current = "current"
            end
        end
        result["type"]="leaf"
        result.id=tostring(item.id)
        result.uri=tostring(path)
        result.name=name
        result.duration=math.floor(item.duration)
        playlist[i] = result
    end
    return playlist

end
httprequests.parseplaylist = parseplaylist

playlisttable = function ()

    local basePlaylist=getplaylist()

    return parseplaylist(basePlaylist)
end
httprequests.playlisttable = playlisttable

getbrowsetable = function ()

    --paths are returned as an array of elements
    local result = { element = { _array = {} } }

    local dir
    --uri takes precedence, but fall back to dir
    if _GET["uri"] then
        if _GET["uri"] == "file://~" then
            dir = "~"
        else
            local uri = string.gsub(_GET["uri"], "[?#].*$", "")
            if not string.match(uri, "/$") then
                uri = uri.."/"
            end
            dir = vlc.strings.make_path(common.realpath(uri))
        end
    elseif _GET["dir"] then
        dir = _GET["dir"]

        -- "" dir means listing all drive letters e.g. "A:\", "C:\"...
        --however the opendir() API won't resolve "X:\.." to that behavior,
        --so we offer this resolution as "backwards compatibility"
        if string.match(dir, '^[a-zA-Z]:[\\/]*%.%.$') or
           string.match(dir, '^[a-zA-Z]:[\\/]*%.%.[\\/]') then
            dir = ""
        end

        if dir ~= "" and dir ~= "~" then
            dir = dir.."/" --luckily Windows accepts '/' as '\'
        end
    end
    if not dir then
        return result
    end

    if dir == "~" then
        dir = vlc.config.homedir().."/"
    end

    local d = vlc.net.opendir(dir)
    table.sort(d)

    --FIXME: this is the wrong place to do this, but this still offers
    --some useful mitigation: see #25021
    if #d == 0 and dir ~= "" then
        table.insert(d, "..")
    end

    for _,f in pairs(d) do
        if f == ".." or not string.match(f,"^%.") then
            local path = dir..f
            local element={}

            local s = vlc.net.stat(path)
            if (s) then
                for k,v in pairs(s) do
                    element[k]=v
                end
            else
                element["type"]="unknown"
            end
            element["path"]=path
            element["name"]=f

            local uri=vlc.strings.make_uri(path)
            if uri then
                element["uri"]=common.realpath(uri)
            end

            table.insert(result.element._array,element)
        end
    end

    return result;
end
httprequests.getbrowsetable = getbrowsetable


getstatus = function (includecategories)


    local item = vlc.player.item()
    local playlist = vlc.object.playlist()
    local vout = vlc.object.vout()
    local aout = vlc.object.aout()

    local s ={}

    --update api version when new data/commands added
    s.apiversion=4
    s.version=vlc.misc.version()
    s.volume=vlc.volume.get()

    s.time = vlc.player.get_time() / 1000000
    s.position = vlc.player.get_position()
    s.currentplid = vlc.playlist.current()
    s.audiodelay = vlc.player.get_audio_delay()
    s.rate = vlc.player.get_rate()
    s.subtitledelay = vlc.player.get_subtitle_delay()

    if item then
        s.length=math.floor(item:duration())
    else
        s.length=0
    end

    if vout then
        s.fullscreen=vlc.var.get(vout,"fullscreen")
        s.aspectratio=vlc.var.get(vout,"aspect-ratio");
        if s.aspectratio=="" then s.aspectratio = "default" end
    else
        s.fullscreen=0
    end

    if aout then
        local filters=vlc.var.get(aout,"audio-filter")
        local temp=strsplit(filters,":")
        s.audiofilters={}
        local id=0
        for i,j in pairs(temp) do
            s.audiofilters['filter_'..id]=j
            id=id+1
        end
    end

    s.videoeffects={}
    s.videoeffects.hue=round(vlc.config.get("hue"),2)
    s.videoeffects.brightness=round(vlc.config.get("brightness"),2)
    s.videoeffects.contrast=round(vlc.config.get("contrast"),2)
    s.videoeffects.saturation=round(vlc.config.get("saturation"),2)
    s.videoeffects.gamma=round(vlc.config.get("gamma"),2)

    s.state=vlc.playlist.status()
    s.random = vlc.playlist.get_random()
    s.loop = vlc.playlist.get_loop()
    s["repeat"] = vlc.playlist.get_repeat()

    s.equalizer={}
    s.equalizer.preamp=round(vlc.equalizer.preampget(),2)
    s.equalizer.bands=vlc.equalizer.equalizerget()
    if s.equalizer.bands ~= null then
        for k,i in pairs(s.equalizer.bands) do s.equalizer.bands[k]=round(i,2) end
        s.equalizer.presets=vlc.equalizer.presets()
    end

    if (includecategories and item) then
        s.information={}
        s.information.category={}
        s.information.category.meta=item:metas()

        local info = item:info()
        for k, v in pairs(info) do
            local streamTable={}
            for k2, v2 in pairs(v) do
                local tag = string.gsub(k2," ","_")
                streamTable[tag]=v2
            end

            s.information.category[k]=streamTable
        end

        s.stats={}

        local statsdata = item:stats()
        for k,v in pairs(statsdata) do
            local tag = string.gsub(k,"_","")
            s.stats[tag]=v
        end

        s.information.chapter = vlc.player.get_chapter_index()
        s.information.title = vlc.player.get_title_index()

        s.information.chapters_count = vlc.player.get_chapters_count()
        s.information.titles_count = vlc.player.get_titles_count()

    end
    return s
end
httprequests.getstatus = getstatus

get_renderers = function()
    local rd = get_renderer_discovery()
    if not rd then
        return {}
    end
    return rd:list()
end
httprequests.get_renderers = get_renderers

_G.httprequests = httprequests
return httprequests
