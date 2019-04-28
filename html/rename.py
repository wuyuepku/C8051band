import shutil, os, sys

DIRECTORY = "piano"
__DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), DIRECTORY)
def __dir(filename):
	return os.path.join(__DIR, filename)

cnt = 0
for filename in os.listdir(__DIR):
	if filename[-18:] == "_爱给网_aigei_com.mp3":
		cnt += 1
		print(filename, "->", filename[:-18] + ".mp3")
		shutil.move(__dir(filename), __dir(filename[:-18] + ".mp3"))

print("%d files renamed in %s" % (cnt, __DIR))
