import shutil, os, sys

DIRECTORY = "mario"
__DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), DIRECTORY)
def __dir(filename):
	return os.path.join(__DIR, filename)

cnt = 0
suffix = "_爱给网_aigei_com.mp3"
for filename in os.listdir(__DIR):
	if filename[-len(suffix):] == suffix:
		cnt += 1
		print(filename, "->", filename[:-len(suffix)] + ".mp3")
		shutil.move(__dir(filename), __dir(filename[:-len(suffix)] + ".mp3"))

print("%d files renamed in %s" % (cnt, __DIR))
