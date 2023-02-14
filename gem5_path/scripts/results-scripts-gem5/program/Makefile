
default: results-scripts.jar

SOURCES=$(shell find -L src/ -type f)

target/scala-2.13/ReportGenerator-assembly-0.1.0-SNAPSHOT.jar: ${SOURCES} build.sbt
	rm -f $@
	sbt assembly

results-scripts.jar: target/scala-2.13/ReportGenerator-assembly-0.1.0-SNAPSHOT.jar
	ln -sf $^ $@

MEM=$(shell printf "24576\\n$$(($$(grep "MemTotal:" /proc/meminfo | tr -s " " | cut -d" " -f2) * 4 / 7 / 1024))\\n" | sort -n | head -n1)

PORT=9056

run:
	sbt -mem $(MEM) "run --port $(PORT)"

clean:
	rm -f results-scripts.jar
	sbt clean
	rm -rf target project/target project/project
