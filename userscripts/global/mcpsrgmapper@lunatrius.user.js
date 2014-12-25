/*jslint devel: true, browser: true, regexp: true, continue: true, plusplus: true, maxerr: 50, indent: 4 */
/*global unsafeWindow: false, $: false, toastr: false, JSZip: false */
/*global GM_setValue: false, GM_getValue: false, GM_listValues: false, GM_deleteValue: false, GM_addStyle: false, GM_getResourceText: false, GM_xmlhttpRequest: false */

// ==UserScript==
// @name           MCP :: SRG Mapper
// @namespace      Lunatrius
// @description    Apply SRG mappings to code.
// @author         Lunatrius <lunatrius@gmail.com>
// @include        https://github.com/MinecraftForge/FML*
// @include        https://github.com/MinecraftForge/MinecraftForge*
// @include        https://github.com/MinecraftForge/FML/pull/*/files
// @include        https://github.com/MinecraftForge/MinecraftForge/pull/*/files
// @include        http://paste.ee/*
// @include        http://paste.feed-the-beast.com/view/*
// @include        http://pastebin.com/*
// @exclude        http://paste.ee/
// @exclude        http://pastebin.com/
// @require        https://ajax.googleapis.com/ajax/libs/jquery/2.1.1/jquery.min.js
// @require        https://raw.githubusercontent.com/defunkt/jquery-pjax/master/jquery.pjax.js
// @require        https://raw.githubusercontent.com/Stuk/jszip/master/dist/jszip.min.js
// @require        https://raw.githubusercontent.com/CodeSeven/toastr/master/build/toastr.min.js
// @require        http://harvesthq.github.io/chosen/chosen.jquery.js
// @resource       toast_css https://raw.githubusercontent.com/CodeSeven/toastr/master/build/toastr.min.css
// @resource       chosen_css http://harvesthq.github.io/chosen/chosen.css
// @updateURL      https://raw.githubusercontent.com/Lunatrius/mustached-dangerzone/master/userscripts/global/mcpsrgmapper@lunatrius.meta.js
// @version        0.1.3
// @grant          GM_setValue
// @grant          GM_getValue
// @grant          GM_listValues
// @grant          GM_deleteValue
// @grant          GM_addStyle
// @grant          GM_getResourceText
// @grant          GM_xmlhttpRequest
// @grant          unsafeWindow
// @license        MIT
// ==/UserScript==

(function (window) {
    "use strict";

    $.fn.outerHtml = function () {
        return $("<tmp>").append(this).html();
    };

    var mapper = {
        baseList: "http://export.mcpbot.bspk.rs/",
        baseMapping: "http://files.minecraftforge.net/maven/de/oceanlabs/mcp/",
        regexParse: /(func_[0-9]+_[a-z]+_?|field_[0-9]+_[a-z]+_?|p_i?[0-9]+_[0-9]+_?),([^,]+),/i,
        regexReplace: /\b(?:func_[0-9]+_[a-z]+_?|field_[0-9]+_[a-z]+_?|p_i?[0-9]+_[0-9]+_?)\b/gi,

        regexMapping: /^(\d+(?:\.\d+)+):((stable)(?:_nodoc)?_(\d+)|(snapshot)(?:_nodoc)?_(\d{8}))$/,

        mappingList: [],
        currentMapping: null,
        mappings: {},

        updateIntervalList: 1000 * 3600 * 12,
        updateIntervalMapping: 1000 * 3600 * 24 * 365,

        settings: {
            "github.com": {
                tag: "li",
                insert: "div.container>ul.pagehead-actions",
                container: "#files table tr td.blob-code, .file .type-diff table tr td.blob-code, .inline-review-comment table tr td.blob-code"
            },
            "paste.ee": {
                tag: "span",
                insert: ".container>.row>.row>.col-sm-12:has(#download)",
                container: "#paste-content ol li div",
                css: [
                    "#remap_container { float: right; }",
                    "#remap_container a { cursor: pointer; }"
                ].join("\n")
            },
            "paste.feed-the-beast.com": {
                tag: "span",
                insert: "div.col-lg-12>div.detail.by",
                container: ".CodeMirror ol li span",
                css: [
                    "#remap_container { float: right; }",
                    "#remap_container a { cursor: pointer; }"
                ].join("\n")
            },
            "pastebin.com": {
                tag: "span",
                insert: "#code_frame #code_buttons",
                container: "#code_frame ol li span",
                css: [
                    "#remap_container { float: right; margin-top: 4pt; }",
                    "#remap_container a { cursor: pointer; }"
                ].join("\n")
            }
        },

        setCurrentMapping: function (mc, type, version) {
            if (type === undefined && version === undefined) {
                var mapping = this.parseMapping(mc);
                this.currentMapping = mapping[0] ? mapping[0] + ":" + mapping[1] + "_" + mapping[2] : null;
            } else {
                this.currentMapping = mc + ":" + type + "_" + version;
            }

            return this.currentMapping;
        },

        parseMapping: function (mapping) {
            var m = mapping && mapping.match(/^(\d+(?:\.\d+)+):((stable)(?:_nodoc)?_(\d+)|(snapshot)(?:_nodoc)?_(\d{8}))$/);
            return m ? [m[1], m[3] || m[5], m[4] || m[6]] : [null, null, null];
        },

        set: function (key, val) {
            GM_setValue(key, JSON.stringify(val));
        },

        get: function (key, def) {
            try {
                return JSON.parse(GM_getValue(key, def)) || def;
            } catch (ignore) {}
            return def;
        },

        list: function () {
            return GM_listValues();
        },

        del: function (key) {
            GM_deleteValue(key);
        },

        clean: function () {
            $.each(this.list(), function (index, value) {
                this.del(value);
            }.bind(this));
        },

        time: function () {
            return new Date().getTime();
        },

        init: function () {
            if (window !== window.top) {
                return;
            }

            var css = [
                GM_getResourceText("toast_css"),
                GM_getResourceText("chosen_css")
            ];

            this.settings = this.settings[location.host];
            if (!this.settings) {
                GM_addStyle(css.join("\n"));
                toastr.info("No settings for " + location.host + "... :(");
                return this;
            }

            if (this.settings.css) {
                css = css.concat(this.settings.css);
            }

            GM_addStyle(css.join("\n"));

            toastr.options.closeButton = true;
            toastr.options.showMethod = "slideDown";
            toastr.options.hideMethod = "slideUp";
            toastr.options.preventDuplicates = true;

            this.setCurrentMapping(this.get("selected", ""));

            this.mappingList = this.get("mappings", []);

            $.each(this.list(), function (index, version) {
                if (this.regexMapping.test(version) && $.inArray(version, this.mappingList) === -1) {
                    toastr.warning("Removing old mappings...<br/>" + version);
                    this.del(version);
                    this.del(version + "_update");
                }
            }.bind(this));

            try {
                this.main();
            } catch (e) {
                console.log(e);
            }

            delete this.init;

            return this;
        },

        main: function () {
            this.update();

            $("<" + this.settings.tag + ">")
                .attr("id", "remap_container")
                .append($("<select>").change(this.changeMapping.bind(this)))
                .append(" ")
                .append($("<a>").addClass("minibutton").text("Remap").click(this.remap.bind(this)))
                .prependTo(this.settings.insert);

            this.populateMappings();
        },

        update: function () {
            if (this.get("mappings_update", 0) + this.updateIntervalList < this.time()) {
                toastr.success("Fetching mapping list...");
                this.query(this.baseList + "versions.json?limit=5", this.saveMappingList.bind(this), this.updateMapping.bind(this));
                return true;
            } else {
                return this.updateMapping();
            }
        },

        updateMapping: function () {
            if (this.setCurrentMapping(this.currentMapping) !== null) {
                if (this.get(this.currentMapping + "_update", 0) + this.updateIntervalMapping < this.time()) {
                    toastr.success("Fetching mappings: " + this.currentMapping);
                    var mapping = this.parseMapping(this.currentMapping);
                    this.query(this.mappingUrl(mapping[0], mapping[1], mapping[2]), this.saveMappings.bind(this), function () {
                        $(this.settings.container).each(this.doRemap.bind(this));
                    }.bind(this));
                    return true;
                } else {
                    this.mappings = this.get(this.currentMapping, []);
                    return false;
                }
            } else {
                toastr.info("No mappings selected.");
            }
            return false;
        },

        changeMapping: function (event) {
            this.setCurrentMapping($(event.target).val());
            this.set("selected", this.currentMapping);
        },

        remap: function (event) {
            var target = $(event.target);

            if (target.hasClass("selected")) {
                target.removeClass("selected");

                $(".processed").removeClass("processed").find("u[title]").each(this.undoRemap);
            } else {
                target.addClass("selected");

                this.setCurrentMapping($("#remap_container select").val());
                this.mappings = this.get(this.currentMapping, []);

                if (!this.update()) {
                    $(this.settings.container).each(this.doRemap.bind(this));
                }
            }

            $("#remap_container .chosen-container").css("z-index", 9001).css("display", target.hasClass("selected") ? "none" : "");
        },

        undoRemap: function (index, node) {
            $(node).replaceWith($(node).attr("title"));
        },

        doRemap: function (index, node) {
            node = $(node);
            if (!node.hasClass("processed")) {
                var html = node.html();
                if (this.regexReplace.test(html)) {
                    node.html(html.replace(this.regexReplace, this.applyRemap.bind(this))).addClass("processed");
                }
            }
        },

        applyRemap: function (token) {
            var mcpname, node;

            mcpname = this.mappings[token];
            node = $("<u>").attr("title", token);

            if (mcpname) {
                return node.text(mcpname).outerHtml();
            }

            return node.append($("<i>").text(token)).outerHtml();
        },

        populateMappings: function () {
            $("#remap_container select").html("");
            $.each(this.mappingList, function (index, version) {
                $("<option>").val(version).text(version).appendTo("#remap_container select");
            });
            $("#remap_container select").val(this.currentMapping).chosen();
        },

        saveMappingList: function (data, callback) {
            var json = JSON.parse(data);
            this.mappingList = [];

            $.each(json, function (mc, data) {
                $.each(data, function (type, versions) {
                    $.each(versions, function (index, version) {
                        this.mappingList.push(mc + ":" + type + "_" + version);
                    }.bind(this));
                }.bind(this));
            }.bind(this));

            this.set("mappings", this.mappingList);
            this.set("mappings_update", this.time());

            toastr.success("Saved mapping list.");

            this.updateMapping();
            this.populateMappings();

            if (callback) {
                callback();
            }
        },

        mappingUrl: function (mc, type, version) {
            return this.baseMapping + "mcp_" + type + "_nodoc/" + version + "-" + mc + "/mcp_" + type + "_nodoc-" + version + "-" + mc + ".zip";
        },

        saveMappings: function (data, callback) {
            var zip, parse;

            zip = JSZip(data);

            parse = function (index, line) {
                var m = line.match(this.regexParse);
                if (m && m.length === 3) {
                    this.mappings[m[1]] = m[2];
                }
            };

            this.mappings = {};
            $.each(zip.file("methods.csv").asText().split("\n"), parse.bind(this));
            $.each(zip.file("fields.csv").asText().split("\n"), parse.bind(this));
            $.each(zip.file("params.csv").asText().split("\n"), parse.bind(this));

            this.set(this.currentMapping, this.mappings);
            this.set(this.currentMapping + "_update", this.time());

            toastr.success("Saved mappings.");

            if (callback) {
                callback();
            }
        },

        query: function (url, callback, innerCallback) {
            GM_xmlhttpRequest({
                url: url,
                method: "GET",
                // should work but doesn't...
                // responseType: "arraybuffer",
                overrideMimeType: "text/plain; charset=x-user-defined",
                onload: function (response) {
                    try {
                        callback(response.response, innerCallback);
                    } catch (e) {
                        toastr.error("Something went wrong!<br/>" + e.message);
                    }
                }.bind(this)
            });
        }
    }.init();
}(unsafeWindow));
