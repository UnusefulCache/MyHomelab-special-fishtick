#!/bin/bash

ESP32_IP="10.152.34.176"
ESP32_PORT=1234
DEVICE="nvme0n1"

declare -A CPU_PREV_TOTAL CPU_PREV_IDLE NET_PREV_RX NET_PREV_TX DISK_PREV_R DISK_PREV_W
prev_time=$(date +%s.%N)

get_cpu_usage() {
  cpu_json=""
  while read -r cpu user nice sys idle iowait irq softirq steal _; do
    [[ $cpu =~ ^cpu[0-9]+$ ]] || continue
    total=$((user + nice + sys + idle + iowait + irq + softirq + steal))
    prev_total=${CPU_PREV_TOTAL[$cpu]:-$total}
    prev_idle=${CPU_PREV_IDLE[$cpu]:-$idle}
    diff_total=$((total - prev_total))
    diff_idle=$((idle - prev_idle))
    usage=0
    (( diff_total > 0 )) && usage=$(awk -v dt=$diff_total -v di=$diff_idle 'BEGIN {printf "%.2f", (1 - di/dt) * 100}')
    cpu_json+=$(printf '"%s": %.2f,' "$cpu" "$usage")
    CPU_PREV_TOTAL[$cpu]=$total
    CPU_PREV_IDLE[$cpu]=$idle
  done < /proc/stat
  echo "${cpu_json%,}"
}

get_net() {
  local net_json=""
  while read -r iface rx _ _ _ _ _ _ tx _; do
    iface=${iface%:}
    [[ $iface == lo ]] && continue
    prev_rx=${NET_PREV_RX[$iface]:-$rx}
    prev_tx=${NET_PREV_TX[$iface]:-$tx}
    NET_PREV_RX[$iface]=$rx
    NET_PREV_TX[$iface]=$tx
    rx_diff=$((rx - prev_rx))
    tx_diff=$((tx - prev_tx))
    [[ $elapsed == 0 ]] && elapsed=1
    rx_kBps=$(awk -v d=$rx_diff -v e=$elapsed 'BEGIN {printf "%.2f", d/1024/e}')
    tx_kBps=$(awk -v d=$tx_diff -v e=$elapsed 'BEGIN {printf "%.2f", d/1024/e}')
    [[ -n $net_json ]] && net_json+=","
    net_json+=$(printf '{"iface":"%s","rx_kBps":%s,"tx_kBps":%s}' "$iface" "$rx_kBps" "$tx_kBps")
  done < <(awk 'NR>2 {print}' /proc/net/dev)
  echo "$net_json"
}

get_mem() {
  awk '/Mem:/ {printf "\"mem_used\": %d, \"mem_free\": %d", $3*1024, $4*1024}' <(free -b)
}

get_disk_usage() {
  df -P /home 2>/dev/null | awk 'NR==2 {
    sub(/%/, "", $5);
    printf "\"disk_home_used_percent\": %s, \"disk_home_free\": \"%s\"", $5, $4
  }'
}

get_disk_io() {
  read r _ _ _ w _ <<<$(cat /sys/block/"$DEVICE"/stat)
  prev_r=${DISK_PREV_R[$DEVICE]:-$r}
  prev_w=${DISK_PREV_W[$DEVICE]:-$w}
  DISK_PREV_R[$DEVICE]=$r
  DISK_PREV_W[$DEVICE]=$w
  rKBs=$(awk -v rr=$r -v pr=$prev_r -v e=$elapsed 'BEGIN {printf "%.2f", (rr-pr)*512/1024/e}')
  wKBs=$(awk -v ww=$w -v pw=$prev_w -v e=$elapsed 'BEGIN {printf "%.2f", (ww-pw)*512/1024/e}')
  printf "\"disk_device\": \"%s\", \"rKBs\": %s, \"wkBs\": %s" "$DEVICE" "$rKBs" "$wKBs"
}

# Initialize once for delta math
get_cpu_usage >/dev/null
awk 'NR>2 {print}' /proc/net/dev >/dev/null
cat /sys/block/"$DEVICE"/stat >/dev/null
sleep 1

while true; do
  now=$(date +%s.%N)
  elapsed=$(awk -v n=$now -v p=$prev_time 'BEGIN {printf "%.3f", n - p}')
  prev_time=$now

  cpu_json=$(get_cpu_usage)
  net_json="\"net\": [$(get_net)]"
  mem_json=$(get_mem)
  disk_json=$(get_disk_usage)
  disk_io_json=$(get_disk_io)
  timestamp=$(date +"%T.%1N")

  json="{${cpu_json}, ${net_json}, ${mem_json}, ${disk_json}, ${disk_io_json}, \"timestamp\": \"${timestamp}\"}"

 # printf "3|%s" "$json" | nc "$ESP32_IP" "$ESP32_PORT"
  printf "5|%s\n" "$json"

  sleep 1
done
