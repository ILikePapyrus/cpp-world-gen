# cpp-world-gen
Simple 2D world generator, configurable via cmd flags (once built).

<details>
<summary><i>Example of a generated Mercator Map with Region Borders</i></summary>
<img src="README_img/_S_6516513.png">
</details>

<details>
<summary><i>Example of a generated heightmap for a Mercator Map</i></summary>
<img src="README_img/_S_6516513_HEIGHTMAP.png">
</details>

<details>
<summary><i>Example of a generated region map for a Mercator Map</i></summary>
<img src="README_img/_S_6516513_REGION.png">
</details>

### Usage
Help window:
```cmd
> ./world-gen.exe --help
Usage: world-gen.exe [options]
Options:
  -w, --width <int>       Map width  (default: 1920)
  -h, --height <int>      Map height (default: 1080)
  -m, --merc-map          Generate a Mercator Map (5632x2160) | OVERRIDES -w && -h
  -s, --seed <int>        Noise seed (default: random)
  -f, --frequency <float> Base noise frequency (default: 0.0003)
      --octaves <int>     Fractal Octaves (default: 5)
      --regions <int>     Number of regions (default: 6)
  -o, --output <path>     Output PNG filename (default: date)
  -d, --output-dir <path> Output directory (default: /filename/)
      --help              Show this message
```

### TODO
- [ ] Fix wrong date string on Win11
- [ ] River Generation
- [ ] Biomes
- [ ] Realistic borders
