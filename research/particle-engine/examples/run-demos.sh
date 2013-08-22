# List of demonstrations
demos=(snow fountains fireworks catherine_wheel galaxy boids ants fish)

# Time to run each demonstration for
if [[ "$1" != "-w" ]] && [[ "$1" != "--wait" ]]; then
	time=30
fi

(
	# Run in sub-shell to suppress stderr garbage from kill
	for d in ${demos[*]}; do
		if [ -n "$time" ]; then
			echo "./$d for $time seconds..."
			(cmdpid=$BASHPID; (sleep $time; kill $cmdpid) & exec ./$d)
		else
			echo "./$d"
			./$d
		fi
	done

	echo "Demonstrations complete"
) 2>/dev/null
