import { Pattern } from "./pattern";

export class Effect {
    patterns: Pattern[] = [];
    isLooping: boolean = false;

    print() {
        for(const data of this.patterns) {
            data.print();
        }
    }

    constructor() {
        this.patterns.push(new Pattern());
    }

    movePattern(from: number, to: number) {
        if (to < 0 || to >= this.patterns.length) {
            return;
        }
        const [moved] = this.patterns.splice(from, 1);
        this.patterns.splice(to, 0, moved);
    }

    deletePattern(index: number) {
        this.patterns.splice(index, 1);
    }

    showRowControls(index: number, onUpdate: () => void) {
        const isFirst = index === 0;
        const isLast = index === this.patterns.length - 1;

        return (
            <div className="pattern-controls">
                <button
                    className="row-btn"
                    title="Move up"
                    disabled={isFirst}
                    onClick={() => {
                        this.movePattern(index, index - 1);
                        onUpdate();
                    }}>
                    ▲
                </button>
                <button
                    className="row-btn"
                    title="Move down"
                    disabled={isLast}
                    onClick={() => {
                        this.movePattern(index, index + 1);
                        onUpdate();
                    }}>
                    ▼
                </button>
                <button
                    className="row-btn row-btn-delete"
                    title="Delete"
                    onClick={() => {
                        this.deletePattern(index);
                        onUpdate();
                    }}>
                    ✕
                </button>
            </div>
        );
    }

    show(index: number, onUpdate: () => void) {
        return (
            <div className="effect-details">
                <h3>Effect Details</h3>

                {this.patterns.length === 0 ? (
                    <p>No patterns added yet.</p>
                ) : (
                    <div className="effect-patterns">
                        {this.patterns.map((pattern, i) => (
                            <div key={i} className="pattern-entry">
                                {pattern.display(i, onUpdate)}
                                {this.showRowControls(i, onUpdate)}
                            </div>
                        ))}
                    </div>
                )}
                <br></br>
                <div>
                    <span>Loop Effect: </span>
                    <input className="loop-checkbox"
                        type="checkbox"
                        checked={this.isLooping}
                        onChange={e => {
                            this.isLooping = e.target.checked;
                            onUpdate();
                        }}
                    />
                </div>

                <div className="effect-buttons">
                    <button
                        onClick={e => {
                            this.patterns.push(new Pattern());
                            onUpdate();
                        }}>
                    +
                    </button>
                    <button
                        onClick={e => {
                            this.patterns.pop();
                            onUpdate();
                        }}>
                    -
                    </button>
                </div>
            </div>
        );
    }
}