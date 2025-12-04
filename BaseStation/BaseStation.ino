<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Robot Grid Map</title>
  <style>
    body { font-family: sans-serif; text-align: center; }
    .grid { display: grid; grid-template-columns: repeat(4, 60px); margin: auto; gap: 5px; }
    .cell { width: 60px; height: 60px; background: #eee; display: flex; align-items: center; justify-content: center; border: 1px solid #ccc; }
    .obstacle { background: red; color: white; }
  </style>
</head>
<body>
  <h2>Robot Grid Map (4x4)</h2>
  <div class="grid" id="grid"></div>

  <script>
    const grid = document.getElementById('grid');
    const cells = [];

    for (let y = 0; y < 4; y++) {
      for (let x = 0; x < 4; x++) {
        const cell = document.createElement('div');
        cell.className = 'cell';
        cell.id = `cell-${x}-${y}`;
        grid.appendChild(cell);
        cells.push(cell);
      }
    }

    async function updateGrid() {
      try {
        const res = await fetch('/map');
        const data = await res.json();

        const cell = document.getElementById(`cell-${data.x}-${data.y}`);
        if (cell) {
          cell.textContent = data.robot;
          cell.className = 'cell' + (data.obstacle ? ' obstacle' : '');
        }
      } catch (e) {
        console.error("Error updating grid:", e);
      }
    }

    setInterval(updateGrid, 2000);
  </script>
</body>
</html>
