(function(h, f) {
    var b = {
            $: function(a) {
                return f.getElementById(a)
            },
            addEvent: function(a, c, b) {
                a.addEventListener ? a.addEventListener(c, b, !1) : a.attachEvent ? a.attachEvent("on" + c, b) : a["on" + c] = b
            },
            getBrowser: function() {
                var a = h.navigator.userAgent.toLocaleLowerCase(),
                    c = /(chrome)\/([\d.]+)/,
                    b = /(firefox)\/([\d.]+)/,
                    e = /(safari)\/([\d.]+)/,
                    g = /(opera)\/([\d.]+)/,
                    f = /(rv):([\d.]+)/,
                    a = a.match(/(msie) ([\d.]+)/) || a.match(c) || a.match(b) || a.match(e) || a.match(g) || a.match(f);
                "undefined" === typeof a &&
                    (a = {
                    name: "unknow",
                    version: "unknow"
                });
                return {
                    name: a[1],
                    version: a[2]
                }
            }
        }, j = function(a) {
            this.options = a;
            this.initDom();
            this.initFixBrowser();
            this.bindEvent();
          //  b.$("h_backurl").value = h.location.href
        };
    j.prototype = {
        wrapper: null,
        initDom: function() {
            this.wrapper = b.$("f-header");
            this.more = b.$("f-more");
            this.moreBox = b.$("f-more-box")
        },
        initFixBrowser: function() {
            "msie" === b.getBrowser().name && (this.moreBox.appendChild(this.getIfr(48, 240)));
        },
        getIfr: function(a, b) {
            var d = f.createElement("div");
            d.className = "f-header-ifr";
            d.innerHTML = '<iframe src="about:blank" frameborder="0" width="' + a + '" height="' + b + '"></iframe>';
            return d
        },
        bindEvent: function() {
            var a = this;
            b.addEvent(this.more, "mouseover", function() {
                a.disMoreBox("block")
            });
            b.addEvent(this.more, "mouseout", function() {
                a.disMoreBox("none")
            })
        },
        disMoreBox: function(a) {
            this.moreBox.style.display = a
        },
    };
    h.head = new j()
})(window, document);