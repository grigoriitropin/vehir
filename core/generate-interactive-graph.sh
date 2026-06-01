#!/usr/bin/env bash
# Generate interactive pan/zoom graph page from live DB state
set -euo pipefail

OUT="docs/interactive-graph.html"
mkdir -p docs

GRAPHS=""
for mode in "" "--ideas" "--programs"; do
    title="Combined"
    [ "$mode" = "--ideas" ] && title="Ideas"
    [ "$mode" = "--programs" ] && title="Programs"
    GRAPH=$(ipm show --md --short $mode 2>/dev/null | sed '1,/```mermaid/d' | sed '/```/,$d' || echo "graph TD\n  empty[\"no data\"]")
    SLUG=$(echo "$title" | tr 'A-Z' 'a-z')
    GRAPHS+="<h2 id='$SLUG'>$title</h2><div class='wrap' id='wrap-$SLUG'><pre class='mermaid'>\n$GRAPH\n</pre></div>\n"
done

cat > "$OUT" << HTML
<!DOCTYPE html><html lang=en><head><meta charset=utf-8>
<title>IPM Interactive Graph</title>
<script src=https://cdn.jsdelivr.net/npm/mermaid@11/dist/mermaid.min.js></script>
<style>
body{background:#0d1117;color:#c9d1d9;font-family:monospace;margin:0;padding:10px}
h2{color:#58a6ff;font-size:13px;margin:16px 0 4px}
.wrap{overflow:hidden;width:100%;position:relative;cursor:grab;
 border:1px solid #30363d;border-radius:6px;background:#161b22}
.mermaid{display:block!important;transform-origin:top left!important}
.mermaid svg{display:block!important;max-width:none!important}
nav{position:sticky;top:0;z-index:10;background:#0d1117;padding:8px 0;
 border-bottom:1px solid #30363d;margin-bottom:8px}
nav a{color:#58a6ff;text-decoration:none;margin-right:12px;font-size:12px}
</style></head><body>
<nav><a href=#combined>Combined</a> <a href=#ideas>Ideas</a> <a href=#programs>Programs</a></nav>
$GRAPHS
<script>
mermaid.initialize({startOnLoad:false,securityLevel:'loose',theme:'dark'});
mermaid.run().then(function(){
 document.querySelectorAll('.wrap').forEach(function(w){
  var m=w.querySelector('.mermaid'), s=m&&m.querySelector('svg');
  if(!s)return;
  var x=0,y=0,z=1,d=false,ax,ay;
  function u(){m.style.transform='translate('+x+'px,'+y+'px) scale('+z+')';}
  w.addEventListener('wheel',function(e){
   e.preventDefault();var f=e.deltaY<0?1.1:0.9;
   if(z*f<.01||z*f>30)return;
   var R=w.getBoundingClientRect();
   x=e.clientX-R.left-(e.clientX-R.left-x)*f;
   y=e.clientY-R.top-(e.clientY-R.top-y)*f;z*=f;u();},{passive:false});
  w.addEventListener('mousedown',function(e){d=true;ax=e.clientX-x;ay=e.clientY-y;w.style.cursor='grabbing';});
  window.addEventListener('mousemove',function(e){if(!d)return;x=e.clientX-ax;y=e.clientY-ay;u();});
  window.addEventListener('mouseup',function(){d=false;w.style.cursor='grab';});
 });
});
</script></body></html>
HTML

echo "Generated $OUT"
