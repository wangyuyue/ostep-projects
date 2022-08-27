# ./wserver [-d basedir] [-p port] [-t threads] [-b buffers] [-s schedalg]
# ./wserver -p 9909 -t 3 -b 5 &
start_time=$(date +%s)
cnt=0
total_time=0
while true; do
  time=$(($RANDOM % 10))
  total_time=$(($total_time + $time)) 
  ./wclient localhost 10000 /spin.cgi\?$time &
cnt=$(($cnt+1))
if [[ cnt -eq 5 ]]; then
break
fi
done;
wait
end_time=$(date +%s)
elapsed=$(( end_time - start_time ))
echo "elapse time $elapsed"
echo "total time $total_time"
