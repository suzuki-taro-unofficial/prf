build/document.pdf: document.md
	mkdir -p build
	pandoc $< -o $@ --pdf-engine=lualatex -V documentclass=ltjarticle

clean:
	rm -rf build
