#!/usr/bin/env python3
"""
ZMK Heatmap Generator
Processes keylog data and generates SVG & interactive HTML heatmaps
integrated directly with Keymap Drawer SVG layouts.
"""

import argparse
import csv
import re
import xml.etree.ElementTree as ET
from pathlib import Path

def parse_args():
    parser = argparse.ArgumentParser(description="ZMK Heatmap SVG & HTML Generator")
    parser.add_argument("--keylog", "-k", default="keylog.csv", help="Input keylog CSV file (default: keylog.csv)")
    parser.add_argument("--svg", "-s", default="keymap-drawer/eyelash_corne.svg", help="Input Keymap Drawer SVG file")
    parser.add_argument("--output-svg", default="heatmap.svg", help="Output Heatmap SVG path (default: heatmap.svg)")
    parser.add_argument("--output-html", default="heatmap.html", help="Output Heatmap HTML report path (default: heatmap.html)")
    parser.add_argument("--title", default="Eyelash Corne Heatmap", help="Heatmap Title")
    return parser.parse_args()

def load_keylog(keylog_path):
    pos_counts = {}
    layer_counts = {}
    total_presses = 0

    if not Path(keylog_path).exists():
        print(f"Warning: Keylog file {keylog_path} not found. Creating sample mock data for demonstration.")
        # Generate representative sample data for Eyelash Corne layout (54 keys)
        sample_counts = {
            4: 1540, 5: 1200, 11: 1890, 12: 1450, 18: 2450, 19: 2100, 20: 3100, 21: 2800,
            26: 2200, 27: 1950, 33: 1320, 34: 1100, 40: 1750, 41: 1600, 47: 4200, 48: 3800,
            49: 2900, 50: 3100
        }
        for pos, count in sample_counts.items():
            pos_counts[pos] = count
            layer_counts[(pos, 0)] = count
            total_presses += count
        return pos_counts, layer_counts, total_presses

    with open(keylog_path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        for row in reader:
            pos = int(row["position"])
            layer = int(row.get("layer", 0))
            pos_counts[pos] = pos_counts.get(pos, 0) + 1
            layer_counts[(pos, layer)] = layer_counts.get((pos, layer), 0) + 1
            total_presses += 1

    return pos_counts, layer_counts, total_presses

def get_heat_color(count, max_count):
    if max_count == 0 or count == 0:
        return {
            "fill": "#16171a",
            "stroke": "#3c3f47",
            "text": "#52525b",
            "glow": "none"
        }

    ratio = count / max_count

    if ratio < 0.15:
        # Cool Cyan / Dark Teal
        return {"fill": "#062d38", "stroke": "#00b8d4", "text": "#00e5ff", "glow": "rgba(0, 229, 255, 0.4)"}
    elif ratio < 0.40:
        # Emerald Green
        return {"fill": "#0a3a22", "stroke": "#00e676", "text": "#69f0ae", "glow": "rgba(0, 230, 118, 0.4)"}
    elif ratio < 0.70:
        # Radiant Yellow / Amber
        return {"fill": "#3d3700", "stroke": "#ffea00", "text": "#ffff8d", "glow": "rgba(255, 234, 0, 0.5)"}
    else:
        # Hot Orange / Red
        return {"fill": "#4a1204", "stroke": "#ff3d00", "text": "#ff8a80", "glow": "rgba(255, 61, 0, 0.6)"}

def generate_svg_heatmap(svg_input_path, svg_output_path, pos_counts, total_presses):
    if not Path(svg_input_path).exists():
        print(f"Error: Input SVG {svg_input_path} does not exist.")
        return False

    with open(svg_input_path, "r", encoding="utf-8") as f:
        svg_content = f.read()

    max_count = max(pos_counts.values()) if pos_counts else 1

    # Inject SVG Heatmap Custom CSS
    heatmap_style = """
    /* ZMK Heatmap Dynamic Inject Styles */
    rect.key.heat-key {
        transition: fill 0.3s ease, stroke 0.3s ease;
    }
    text.heat-badge {
        font-family: Ubuntu Mono, Consolas, monospace;
        font-size: 9px;
        font-weight: bold;
    }
    """
    svg_content = svg_content.replace("</style>", f"{heatmap_style}\n</style>")

    # Parse key elements in SVG using regex to preserve SVG structure & SVG layout formatting
    # Regex matches key groups: <g ... class="... keypos-(\d+) ..."> ... </g>
    def update_key_group(match):
        g_tag = match.group(1)
        pos = int(match.group(2))
        inner_content = match.group(3)

        count = pos_counts.get(pos, 0)
        colors = get_heat_color(count, max_count)

        # Update rect.key inside inner_content
        def update_rect(r_match):
            r_attrs = r_match.group(1)
            # Override fill and stroke
            new_style = f'fill="{colors["fill"]}" stroke="{colors["stroke"]}" stroke-width="1.8px" style="filter: drop-shadow(0px 0px 4px {colors["glow"]});"'
            return f'<rect {r_attrs} class="key heat-key" {new_style}/>'

        # Replace main key rect (not side rect)
        inner_updated = re.sub(r'<rect([^>]+class="key"(?! side)[^>]*)/>', update_rect, inner_content)

        # Add keypress badge if pressed
        if count > 0:
            badge = f'\n<text x="0" y="21" class="heat-badge" fill="{colors["text"]}" text-anchor="middle">{count:,}</text>'
            inner_updated += badge

        return f'<g{g_tag} class="key keypos-{pos}">{inner_updated}</g>'

    # Process key groups
    pattern = re.compile(r'<g([^>]+)class="[^"]*keypos-(\d+)[^"]*"[^>]*>(.*?)</g>', re.DOTALL)
    modified_svg = pattern.sub(update_key_group, svg_content)

    with open(svg_output_path, "w", encoding="utf-8") as f:
        f.write(modified_svg)

    print(f"Heatmap SVG saved to {svg_output_path}")
    return modified_svg

def generate_html_report(html_output_path, svg_content, pos_counts, total_presses, title):
    max_count = max(pos_counts.values()) if pos_counts else 1
    unique_keys = len(pos_counts)
    most_used_pos = max(pos_counts, key=pos_counts.get) if pos_counts else 0
    most_used_count = pos_counts.get(most_used_pos, 0)

    # Build stats table rows
    table_rows = []
    sorted_pos = sorted(pos_counts.items(), key=lambda x: x[1], reverse=True)
    for pos, count in sorted_pos:
        pct = (count / total_presses * 100) if total_presses > 0 else 0
        colors = get_heat_color(count, max_count)
        table_rows.append(f"""
        <tr>
            <td><strong>Keypos {pos}</strong></td>
            <td><span class="badge" style="background:{colors['fill']}; color:{colors['text']}; border:1px solid {colors['stroke']}">{count:,}</span></td>
            <td>
                <div class="progress-bar-bg">
                    <div class="progress-bar-fill" style="width: {pct:.1f}%; background: {colors['stroke']};"></div>
                </div>
            </td>
            <td>{pct:.2f}%</td>
        </tr>
        """)

    table_html = "\n".join(table_rows)

    html_template = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{title} - ZMK Heatmap</title>
    <style>
        :root {{
            --bg-color: #0f1013;
            --card-bg: #16171a;
            --border-color: #27272a;
            --text-main: #f4f4f5;
            --text-muted: #a1a1aa;
            --accent-blue: #00e5ff;
        }}
        body {{
            margin: 0;
            padding: 24px;
            background-color: var(--bg-color);
            color: var(--text-main);
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
        }}
        .container {{
            max-width: 1200px;
            margin: 0 auto;
        }}
        header {{
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid var(--border-color);
            padding-bottom: 16px;
            margin-bottom: 24px;
        }}
        h1 {{
            margin: 0;
            font-size: 24px;
            color: var(--accent-blue);
            display: flex;
            align-items: center;
            gap: 12px;
        }}
        .stats-grid {{
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 16px;
            margin-bottom: 24px;
        }}
        .stat-card {{
            background: var(--card-bg);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            padding: 16px;
        }}
        .stat-card .label {{
            font-size: 12px;
            color: var(--text-muted);
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }}
        .stat-card .value {{
            font-size: 28px;
            font-weight: bold;
            margin-top: 4px;
            color: #ffffff;
        }}
        .heatmap-container {{
            background: var(--card-bg);
            border: 1px solid var(--border-color);
            border-radius: 12px;
            padding: 24px;
            display: flex;
            justify-content: center;
            align-items: center;
            overflow-x: auto;
            margin-bottom: 32px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.4);
        }}
        .heatmap-container svg {{
            max-width: 100%;
            height: auto;
        }}
        .stats-section {{
            background: var(--card-bg);
            border: 1px solid var(--border-color);
            border-radius: 12px;
            padding: 24px;
        }}
        table {{
            width: 100%;
            border-collapse: collapse;
            margin-top: 16px;
        }}
        th, td {{
            padding: 12px;
            text-align: left;
            border-bottom: 1px solid var(--border-color);
        }}
        th {{
            color: var(--text-muted);
            font-size: 13px;
        }}
        .badge {{
            padding: 4px 8px;
            border-radius: 4px;
            font-family: monospace;
            font-size: 12px;
        }}
        .progress-bar-bg {{
            background: #27272a;
            height: 8px;
            border-radius: 4px;
            overflow: hidden;
            width: 100%;
        }}
        .progress-bar-fill {{
            height: 100%;
            border-radius: 4px;
        }}
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>🔥 {title}</h1>
            <div style="font-size:14px; color: var(--text-muted);">ZMK Firmware Heatmap Generator</div>
        </header>

        <div class="stats-grid">
            <div class="stat-card">
                <div class="label">Total Keypresses</div>
                <div class="value">{total_presses:,}</div>
            </div>
            <div class="stat-card">
                <div class="label">Unique Keys Actuated</div>
                <div class="value">{unique_keys}</div>
            </div>
            <div class="stat-card">
                <div class="label">Top Keypos</div>
                <div class="value">Pos {most_used_pos}</div>
            </div>
            <div class="stat-card">
                <div class="label">Top Keypress Count</div>
                <div class="value">{most_used_count:,}</div>
            </div>
        </div>

        <div class="heatmap-container">
            {svg_content}
        </div>

        <div class="stats-section">
            <h2>📊 Keypress Distribution Analysis</h2>
            <table>
                <thead>
                    <tr>
                        <th>Key Position</th>
                        <th>Press Count</th>
                        <th>Frequency Share</th>
                        <th>Percentage</th>
                    </tr>
                </thead>
                <tbody>
                    {table_html}
                </tbody>
            </table>
        </div>
    </div>
</body>
</html>
"""

    with open(html_output_path, "w", encoding="utf-8") as f:
        f.write(html_template)

    print(f"Interactive Heatmap HTML saved to {html_output_path}")

def main():
    args = parse_args()
    pos_counts, layer_counts, total_presses = load_keylog(args.keylog)
    print(f"Loaded keylog: {total_presses:,} total presses across {len(pos_counts)} key positions.")

    svg_content = generate_svg_heatmap(args.svg, args.output_svg, pos_counts, total_presses)
    if svg_content:
        generate_html_report(args.output_html, svg_content, pos_counts, total_presses, args.title)

if __name__ == "__main__":
    main()
