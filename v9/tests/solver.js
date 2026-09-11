import { inputPuzzle } from "../src/puzzles.js";
export function stackMove(p) {
  let best = null;
  for (let rot = 0; rot < 2; rot++)
    for (let col = 0; col < (rot ? 5 : 6); col++) {
      const q = structuredClone(p);
      q.cursor = col;
      q.rotation = rot;
      inputPuzzle(q, "confirm");
      let value = (p.remaining - q.remaining) * 1000;
      const occupied = q.board.filter(Boolean).length;
      let height = 0;
      for (let i = 0; i < 60; i++)
        if (q.board[i]) height += 10 - Math.floor(i / 6);
      value -= height * 2 + occupied * 4;
      for (let i = 0; i < 60; i++)
        if (q.board[i]) {
          if (i % 6 < 5 && q.board[i + 1]?.color === q.board[i].color)
            value += 9;
          if (i < 54 && q.board[i + 6]?.color === q.board[i].color) value += 9;
        }
      if (!best || value > best.value) best = { rot, col, value };
    }
  return best;
}
