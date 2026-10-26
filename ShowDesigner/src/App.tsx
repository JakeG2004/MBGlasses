import { useState, useRef } from "react";
//import { invoke } from "@tauri-apps/api/core";
import "./App.css";
import { Pattern } from "./pattern";
import { Effect } from "./effect"

const KEYS = [
  ["1", "2", "3", "A"],
  ["4", "5", "6", "B"],
  ["7", "8", "9", "C"],
  ["*", "0", "#", "D"],
];

function App() {
  const [effects, setEffects] = useState<Effect[]>(() => 
    Array.from({ length: 16 }, () => new Effect())
  );

  const [selectedEffect, setSelectedEffect] = useState<Effect | null>(null);

  return (
    <main className="container">
      <h1>Welcome to the Prism VMB Show Designer</h1>

      <div>
        <table>
          <tbody>
            {KEYS.map((row, r) => (
              <tr key={r}>
                {row.map((key, index) => {
                  const isDigit = /^[0-9]$/.test(key);
                  const effectIndex = (r * 4) + index; 

                  return (
                    <td key={key}>
                      <button
                        type="button"
                        className={`keypad-btn ${isDigit ? "key-blue" : "key-red"}`}
                        onClick={() => setSelectedEffect(effects[effectIndex])}
                      >
                        {key}
                      </button>
                    </td>
                  );
                })}
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      <div className="display-area">
        {selectedEffect ? selectedEffect.show(0, () => setEffects([...effects])) : <p>Click a button to view effect</p>}
      </div>
    </main>
  );
}

export default App;