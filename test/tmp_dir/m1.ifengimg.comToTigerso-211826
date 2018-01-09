/**
 * iis front js
 * yaoya 2017/1/26 15:42:19
 * An+ti shie+lding
*/
(function () {
	var iis_version = '1.3';
	if (typeof(window.iis) == 'undefined' || window.iis.version != iis_version) {
		window.iis = {
			version: iis_version,
			click_target: '_blank',
			
			// call iis front
			// demo: http://iis1.deliver.ifeng.com/getcode?ap=1234&tp=1&w=1000&h=90&dm=news.ifeng.com&cb=iis.display&rb=12&rc=1-2&ip=8.8.8.8
			show: function(p) {
				var url = 'http://iis1.deliver.ifeng.com';
				url += '/getcode';
				url += '?ap=' + p.ap;
				url += '&tp=' + p.tp;
				url += '&w=' + p.w;
				url += '&h=' + p.h;
				url += '&dm=' + this.get_domain();
				url += '&cb=' + 'iis.display';
				if (typeof(p.rb) != 'undefined') {
					url += '&rb=' + p.rb;
				}
				if (typeof(p.rc) != 'undefined') {
					url += '&rc=' + p.rc;
				}
				if (typeof(p.isrf) != 'undefined') {
					url += '&rf=' + encodeURIComponent(document.referrer);
				}
				url += '&c=' + new Date().getTime();
				
				if (p.tp == 1 && !this.is_mobile()) {
					if (typeof(p.isal) != 'undefined') { // need a logo
						this.isal = p.isal;
					} else {
						this.isal = 1;
					}
					if (typeof(p.iswh) != 'undefined') { // need fix width and height
						this.iswh = p.iswh;
					} else {
						this.iswh = 0;
					}
				}
				
				document.write('<scr'+'ipt src = "' + url + '" ><\/scr'+'ipt>');
			},

			// display code
			// demo: iis.display({ap : 'testau1', w : 1000, h : 90, rid : 4, code : '...');
			display: function(p) {
				try {
					var span = '<span id="iis_ap_' + p.ap + '" style="display:none;"></span>'+"\n";
					if (typeof(p.code) != 'undefined') {
						span += '<div id="iis_al_' + p.ap + '" style="position:relative;padding:0;'+((!this.is_mobile() && this.iswh)?('margin:0 auto;width:'+p.w+'px;height:'+p.h+'px;'):'margin:0;')+'">'+"\n";
						var code = '';
						if (p.ct == 'rtb' || p.ct == 'pdb') { // rtb
							if (this.is_mobile()) {
								this.click_target = '_top';
							}
							if (p.t == '1') { // static
								for (var i = 0; i < p.code.murl.length; i++) {
									this.m_impression(p.code.murl[i]);
								}
								
								var click_split = p.code.curl.indexOf("&u=") + 3;
								var iis_curl = p.code.curl.substring(0, click_split);
								var ids_curl = p.code.curl.substring(click_split);
								var pre_host = location.host.split("\.")[0];
    							pre_host = pre_host.toLowerCase();
    							var is_home = false;
    							var is_top = false;
    							var is_article = false;
    							if (pre_host == 'i' || pre_host == '3g' || pre_host == 'wap') {
    								is_home = true;
    							}
    							if (location.pathname == '/' || location.pathname == '/index.shtml') {
    								is_top = true;
    							}
    							var gs = null;
    							try {
	    							if (typeof(gloableSettings) != "undefined") {
	    								gs = gloableSettings;
	    							} else if (typeof(window.parent.gloableSettings) != "undefined") {
	    								gs = window.parent.gloableSettings;
	    							}
	    						} catch (e) {;}
    							if (gs != null && gs.docType == 'article') {
    								is_article = true;
    							}
    							
								if (p.code.stype == 3) { // feed
									var feed_js = 'http://c0.ifengimg.com/iis/iis_feed.js';
									if (p.ap == 16292) { // pic feed
										feed_js = 'http://c0.ifengimg.com/iis/iis_feed_pic.js';
									} else if (p.ap == 18689) { // custom feed
										feed_js = 'http://c0.ifengimg.com/iis/iis_feed_18689.js';
									} else if (pre_host == 'tv' && is_top) { // tv top feed fix
										feed_js = 'http://c0.ifengimg.com/iis/iis_feed_tv.js';
									} else if (is_home) { // home feed
										feed_js = 'http://c0.ifengimg.com/iis/iis_feed_home.js';
									} else if (is_article) { // article feed
										feed_js = 'http://c0.ifengimg.com/iis/iis_feed_article.js';
										if (p.ap == 18510) {
											feed_js = 'http://c0.ifengimg.com/iis/iis_feed_article_18510.js';
										}
									} else if (is_top && (
										   pre_host == 'inews'
										|| pre_host == 'ifinance'
										|| pre_host == 'ient'
										|| pre_host == 'imil'
										|| pre_host == 'isports'
										|| pre_host == 'ifashion'
										|| pre_host == 'itech'
										|| pre_host == 'istock'
										|| pre_host == 'ihistory'
										|| pre_host == 'iastro')) { // channel top feed
										feed_js = 'http://c0.ifengimg.com/iis/iis_feed_channel.js';
									}
									code += '<div id="iis_rtbs_' + p.ap + '" style="position:relative;margin:0;padding:0;width:100%;">'+"\n";
									code += '<script type="text/javascript">'+"\n";
									code += 'var a = "'+p.code.aurl+'";'+"\n";
									code += 'var h = "'+p.code.h+'";'+"\n";
									code += 'var t = "'+p.code.text+'";'+"\n";
									code += 'var ct = "'+this.click_target+'";'+"\n";
									if (p.b == 'ids') { // ids click first
										code += 'var c = "'+ids_curl+'";'+"\n";
										code += 'var ac = "'+iis_curl+'";'+"\n";
									} else {
										code += 'var c = "'+p.code.curl+'";'+"\n";
										code += 'var ac = "";'+"\n";
									}
									code += '</script>'+"\n";
									code += '<script type="text/javascript" src="' + feed_js + '"></script>'+"\n";
									code += '</div>'+"\n";
								} else if (p.code.stype == 2) { // text
									var text_js = 'http://c0.ifengimg.com/iis/iis_text.js';
									if (is_article) { // article text
										text_js = 'http://c0.ifengimg.com/iis/iis_text_article.js';
									}
									code += '<div id="iis_rtbs_' + p.ap + '" style="position:relative;margin:0;padding:0;width:100%;overflow:hidden;">'+"\n";
									code += '<script type="text/javascript">'+"\n";
									code += 'var a = "'+p.ap+'";'+"\n";
									code += 'var h = "'+p.code.h+'";'+"\n";
									code += 'var t = "'+p.code.text+'";'+"\n";
									code += 'var ct = "'+this.click_target+'";'+"\n";
									if (p.b == 'ids') { // ids click first
										code += 'var c = "'+ids_curl+'";'+"\n";
										code += 'var ac = "'+iis_curl+'";'+"\n";
									} else {
										code += 'var c = "'+p.code.curl+'";'+"\n";
										code += 'var ac = "";'+"\n";
									}
									code += '</script>'+"\n";
									code += '<script type="text/javascript" src="' + text_js + '"></script>'+"\n";
									code += '</div>'+"\n";
								} else { // banner
									if (this.is_mobile()) {
										if (p.b == 'ids') { // ids click first
											code += '<div style="width:30px; height:10px; border:1px solid #bababa; border-radius:2px; font-size:10px; text-align:center; position:absolute; right:4px; bottom:4px; color:#bababa; line-height:9px; z-index: 999;">广告</div>'+"\n";
											code += '<a target="'+this.click_target+'" href="' + ids_curl + '" onclick="iis.m_click('+"'"+ iis_curl +"'"+');"><img src="' + p.code.aurl + '" style="width:100%;border:none;"/></a>'+"\n";
										} else {
											code += '<div style="width:30px; height:10px; border:1px solid #bababa; border-radius:2px; font-size:10px; text-align:center; position:absolute; right:4px; bottom:4px; color:#bababa; line-height:9px; z-index: 999;">广告</div>'+"\n";
											code += '<a target="'+this.click_target+'" href="' + p.code.curl + '"><img src="' + p.code.aurl + '" style="width:100%;border:none;"/></a>'+"\n";
										}
									} else {
										code += '<div id="iis2_rtbs_' + p.ap + '" style="position:relative;margin:0;padding:0;width:'+p.w+'px;height:'+p.h+'px;">'+"\n";
										if (/\.swf$/.test(p.code.aurl.toLowerCase())) {
											var flash_nad = '<embed  width="'+p.w+'px" height="'+p.h+'px" wmode="opaque" src="'+p.code.aurl+'" allowScriptAccess="always" type="application/x-shockwave-flash"></embed>';
								    		if (/msie/.test(navigator.userAgent.toLowerCase())) {
								    		    flash_nad = '<object  width="'+p.w+'px" height="'+p.h+'px" classid="clsid:D27CDB6E-AE6D-11cf-96B8-444553540000" '
								    		        +'codebase="http://download.macromedia.com/pub/shockwave/cabs/flash/swflash.cab#version=6,0,29,0" id="iis2_flash_swf_'+p.ap+'">'
								    		        +'<param name="wmode" value="opaque"/><param name="allowScriptAccess" value="always"><param name="movie" value="'+p.code.aurl+'">'+flash_nad+'</object>';
								    		}
								    		code += '<div style="clear: both; margin: 0 auto; width:'+p.w+'px;height:'+p.h+'px;" id="iis2_flash_outer_2_'+p.ap+'">'
								    		    +'<div style="width:'+p.w+'px;height:'+p.h+'px;position:relative;" id="iis2_flash_outer_1_'+p.ap+'">'+flash_nad;
								    		code += '<div style="width:' + p.w +'px;position:absolute; top:0px; left:0px;z-index:3;">'
							    			    +'<a href="'+p.code.curl+'" target="'+this.click_target+'"><img style="width:'+p.w+'px;height:'+p.h+'px;border:0px" '
							    			    +'src="http://c1.ifengimg.com/mappa1x1/2017/07/27/1x1.gif"></a></div>';
							    			code += '</div></div><div class="clear"></div>';
										} else {
											code += '<a target="'+this.click_target+'" href="' + p.code.curl + '"><img src="' + p.code.aurl + '" style="width:' + p.w + 'px;height:' + p.h + 'px;border:none;"/></a>';
										}
										
										 // logo
										if (p.b == 'zsjq') { // zhong shi jin qiao
											code += '<img src="http://y2.ifengimg.com/6e99b17a6eb74b14/2015/1113/rdn_5645570d8ff01.png" style="width:20px;height:20px;position:absolute;z-index:10;left:'+(p.code.w-20)+'px;top:'+(p.code.h-20)+'px;background:#ffffff;filter:alpha(Opacity=70);-moz-opacity:0.7;opacity:0.7;" />'+"\n";
										}
										if (p.b == 'xs') { // xin shu
											code += '<img src="http://c1.ifengimg.com/mappa/2015/12/18/cf10c6f5cfbc1a7d7ca30087ddc17ecf.png" style="width:50px;height:20px;position:absolute;z-index:10;left:'+(p.code.w-50)+'px;top:'+(p.code.h-20)+'px;background:#ffffff;filter:alpha(Opacity=70);-moz-opacity:0.7;opacity:0.7;" />'+"\n";
										}
										code += '</div>'+"\n";
									}
								}
							} else { // dynamic 
								var pcode = '';
								if (typeof(p.code) == 'object') {
									pcode = p.code.html;
									for (var i = 0; i < p.code.murl.length; i++) {
										this.m_impression(p.code.murl[i]);
									}
								} else {
									pcode = p.code;
								}
								if (!this.is_mobile() && p.b == 'xs') {
									code += '<div id="iis_rtbs_' + p.ap + '" style="position:relative;margin:0;padding:0;width:'+p.w+'px;height:'+p.h+'px;">'+"\n";
									code += pcode;
									code += '<img src="http://c1.ifengimg.com/mappa/2015/12/18/cf10c6f5cfbc1a7d7ca30087ddc17ecf.png" style="width:50px;height:20px;position:absolute;z-index:10;left:'+(p.w-50)+'px;top:'+(p.h-20)+'px;background:#ffffff;filter:alpha(Opacity=70);-moz-opacity:0.7;opacity:0.7;" />'+"\n";
									code += '</div>'+"\n";
								} else {
									code += pcode;
								}
							}
						} else {
							code = decodeURIComponent(p.code.replace(/\+/g, "%20")); // decode java URLEncoder.encode
						}
						span += code;
						if (this.isal) { // guang gao text
							span += '<img src="http://y2.ifengimg.com/ifengimcp/pic/20150902/3677f2773fd79f12b079_size1_w35_h15.png" ';
							span += 'style="width:35px;height:15px;position:absolute;z-index:12;left:0px;';
							span += 'top:'+(p.h-15)+'px;left:'+((this.isal=='2')?(p.w-35):0)+'px;';
							span += 'background:#ffffff;filter:alpha(Opacity=100);-moz-opacity:1;opacity:1;border:0;" />'+"\n";
						}
						span += '</div>'+"\n";
					}
					document.write(span);
					this.isal = 0;
					this.iswh = 0;

					// do cookie mapping
					if (p.hasOwnProperty('curl')) {
						if (p.curl.indexOf('http') >= 0) {
							document.write('<img src="'+p.curl+'" width=1 height=1 style="display:none;" />');
						}
					}
				} catch (e) {
					;
				}
			},

			// get current domain, support iframe once.
			// (but, referrer is not sure, the request flow return, you should use the cookie transfer.)
			// bugfix, comment parent for 'blocked a frame with origin' error.
			get_domain: function() {
				var dm = location.hostname;
				// gm target
				var is_gm_v = this.get_cookie('is_ga'+'me_v');
				if (is_gm_v == '1') {
					dm = 'from.ga'+'mes.ifeng.com';
				}
				// baidu target
				if (this.is_mobile() && (navigator.userAgent.match(/(baidubrowser|baiduboxapp)/i))) {
					dm = 'baidu.ua';
				}
				// uc target
				if (this.is_mobile() && (navigator.userAgent.match(/(ucbrowser|ucweb)/i))) {
					dm = 'uc.ua';
				}
				// qq target
				if (this.is_mobile() && (navigator.userAgent.match(/qqbrowser/i))) {
					dm = 'qq.ua';
				}
				return dm;
			},

			// set cookies
			set_cookie: function(name, value, expire) {
				var date = new Date();
				var eStr = '';
				if ("undefined" != typeof expire && expire != "") {
					expire = new Date(date.getTime() + expire * 60000);
					eStr = 'expires=' + expire.toGMTString() + ';'
				}
				document.cookie = name + '=' + escape(value) + ';path=/;' + eStr;
			},

			// get cookie
			get_cookie: function(name) {
				var value = '';
				var part;

				var pairs = document.cookie.split('; ');
				for (var i = 0; i < pairs.length; i++) {
					part = pairs[i].split('=');
					if (part[0] == name) {
						value = unescape(part[1]);
					}
				}
				return value;
			},
			
			m_impression: function (url) {
				var tmr = new Date().getTime()+parseInt(Math.random()*10000);
				if (url.indexOf('?') > 0) {
					url += '&tmr='+tmr;
				} else {
					url += '?tmr='+tmr;
				}
				var img = new Image(1,1);
				img.onload = img.onerror = function() {};
				img.src = url;
				return 1;
			},

			m_click: function (url) {
				var img = new Image(1,1);
				img.onload = img.onerror = function() {};
				img.src = url;
				return 1;
			},
						
			load_script: function (url) {
				var script = document.createElement("script");
				script.type = "text/javascript";
				script.src = url;
				document.body.appendChild(script);
			},
			
			is_mobile: function() {
				if ((navigator.userAgent.match(/(phone|pad|pod|ios|android|mobile|blackberry|mqqbrowser|juc|fennec|wosbrowser|browserng|webos|symbian)/i))) {
					return true;
				}
				if (location.hostname.substr(0,1) == 'i') {
					return true;
				}
				return false;
			},
			
			is_ie6: function() {
				return /msie 6/.test(navigator.userAgent.toLowerCase());
			}
		};
	}
	try {
		iis.show(window.iis_config);
	} catch(e) {}
})();
