#!/bin/sh
# DXVKit encoder vs the ffmpeg binary as a black-box DXV decoder.
#
#   tools/oracle.sh DXVTOOL WORKDIR [WxH ...]
#
# For each size: a synthetic source (FFmpeg's testsrc2 pattern, 2 frames, alpha = opaque top third,
# then a horizontal ramp), encoded with dxvtool for every variant and effort, decoded by ffmpeg
# (-v warning: any output line counts as a warning) and by DXVKit, then compared:
#   YCG6 / YG10: ffmpeg's native yuv(a)420p planes (U = Cg, V = Co) against DXVKit's sample planes:
#                must be identical.
#   DXT1:        RGBA; differences of at most 1 (the BC1 interpolation / expansion convention).
#   DXT5:        RGBA, DXVKit's colour un-premultiplied first (ffmpeg's DXT5 output is
#                floor(c x 255 / a), measured); alpha must be identical; the opaque-source DXT5 file
#                must differ by at most 1.
# ffmpeg 8.1.1 sizes the DXV texture with the width rounded up to 4, not 16 (measured: it decodes a
# 908-wide stream as 227 blocks per row, Resolume's own 908 x 2276 DXT1 file included). Where
# ceil4(W) != ceil16(W) its output is therefore not a reference: those rows are reported as
# "ffmpeg geometry" and not counted as failures.
# Only the ffmpeg / ffprobe executables are used, as external programs. Nothing of FFmpeg is linked
# or committed.
#
# MIT License. Copyright (c) 2026 Amrit Rosell. See LICENSE.
set -u
T=$1
W=$2
shift 2
SIZES=${*:-"16x16 18x10 333x221 1920x1080 908x2276 3840x2160 1x1"}
mkdir -p "$W"
fail=0
printf '%-10s %-5s %-5s %-6s %-9s %s\n' size var eff frames warnings comparison
for S in $SIZES; do
  w=${S%x*}; h=${S#*x}
  ew=$(( (w + 15) / 16 * 16 )); eh=$(( (h + 15) / 16 * 16 ))
  src="$W/src_$S.rgba"; opq="$W/opq_$S.rgba"
  ffmpeg -v error -y -f lavfi -i "testsrc2=size=${ew}x${eh}:rate=30" -frames:v 2 \
    -vf "format=rgba,crop=$w:$h:0:0,geq=r='r(X,Y)':g='g(X,Y)':b='b(X,Y)':a='if(lt(Y,H/3),255,X*255/W)'" \
    -f rawvideo -pix_fmt rgba "$src" || { echo "source $S failed"; fail=1; continue; }
  ffmpeg -v error -y -f lavfi -i "testsrc2=size=${ew}x${eh}:rate=30" -frames:v 2 \
    -vf "format=rgba,crop=$w:$h:0:0" -f rawvideo -pix_fmt rgba "$opq"
  fb=$(( w * h * 4 )); cw=$(( (w + 1) / 2 )); ch=$(( (h + 1) / 2 ))
  geometry=ok
  [ $(( (w + 3) / 4 * 4 )) = $ew ] || geometry=ffmpeg
  for V in dxt1 dxt5 ycg6 yg10 dxt5-opaque; do
    for E in fast best; do
      in=$src; var=$V
      [ "$V" = dxt5-opaque ] && { in=$opq; var=dxt5; }
      mov="$W/${V}_${E}_$S.mov"
      "$T" encode "$in" "$mov" --size "$S" --variant $var --effort $E > /dev/null || { fail=1; continue; }
      frames=$(ffprobe -v error -count_frames -select_streams v -show_entries stream=nb_read_frames -of csv=p=0 "$mov")
      result=""
      if [ $geometry = ffmpeg ]; then
        warn=$(ffmpeg -v warning -y -i "$mov" -f null - 2>&1 | grep -c . | tr -d ' ')
        printf '%-10s %-5s %-5s %-6s %-9s %s\n' "$S" "$V" "$E" "$frames" "$warn" \
          "ffmpeg geometry: width $w is 4-aligned to $(( (w + 3) / 4 * 4 )) by ffmpeg, coded width is $ew (not a reference)"
        continue
      fi
      for f in 0 1; do
        case $var in
        ycg6|yg10)
          [ $var = ycg6 ] && pf=yuv420p || pf=yuva420p
          warn=$(ffmpeg -v warning -y -i "$mov" -f rawvideo -pix_fmt $pf "$W/ff.yuv" 2>&1 | wc -l | tr -d ' ')
          ys=$(( w * h )); cs=$(( cw * ch ))
          [ $var = ycg6 ] && fs=$(( ys + 2 * cs )) || fs=$(( 2 * ys + 2 * cs ))
          dd if="$W/ff.yuv" of="$W/fff.yuv" bs=$fs skip=$f count=1 2>/dev/null
          "$T" dump-samples "$mov" $f "$W/our.yuv" > /dev/null
          # ours: Y Co Cg [A]; ffmpeg: Y U=Cg V=Co [A]
          head -c $ys "$W/our.yuv" > "$W/oo.yuv"
          tail -c +$(( ys + cs + 1 )) "$W/our.yuv" | head -c $cs >> "$W/oo.yuv"
          tail -c +$(( ys + 1 )) "$W/our.yuv" | head -c $cs >> "$W/oo.yuv"
          [ $var = yg10 ] && tail -c +$(( ys + 2 * cs + 1 )) "$W/our.yuv" >> "$W/oo.yuv"
          d=$(cmp -l "$W/fff.yuv" "$W/oo.yuv" 2>/dev/null | wc -l | tr -d ' ')
          [ "$d" = 0 ] && r="planes identical" || { r="planes differ in $d bytes"; fail=1; }
          ;;
        *)
          warn=$(ffmpeg -v warning -y -i "$mov" -f rawvideo -pix_fmt rgba "$W/ff.rgba" 2>&1 | wc -l | tr -d ' ')
          dd if="$W/ff.rgba" of="$W/fff.rgba" bs=$fb skip=$f count=1 2>/dev/null
          "$T" dump-rgba "$mov" $f "$W/our.rgba" > /dev/null
          if [ $V = dxt5 ]; then
            r=$("$T" cmp-rgba "$W/fff.rgba" "$W/our.rgba" --size "$S" --unpremultiply-b)
          else
            r=$("$T" cmp-rgba "$W/fff.rgba" "$W/our.rgba" --size "$S")
          fi
          r=${r#*: }
          # DXT1 / opaque DXT5: |d| <= 1; DXT5 with alpha: alpha must match
          case $V in
          dxt5) echo "$r" | grep -q "A 0$" || fail=1 ;;
          *) echo "$r" | grep -q ">2: 0) | max |d| R [01] G [01] B [01] A 0" || fail=1 ;;
          esac
          ;;
        esac
        [ "$warn" = 0 ] || fail=1
        result="$result f$f: $r;"
      done
      printf '%-10s %-5s %-5s %-6s %-9s %s\n' "$S" "$V" "$E" "$frames" "$warn" "$result"
    done
  done
done
[ $fail = 0 ] && echo "ORACLE: all files decoded by ffmpeg without warnings and matched" || echo "ORACLE: FAILURES"
exit $fail
