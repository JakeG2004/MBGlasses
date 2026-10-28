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

    show(index: number, onUpdate: () => void) {
        return (
            <div className="effect-details">
                <h3>Effect Details</h3>
                
                {this.patterns.length === 0 ? (
                    <p>No patterns added yet.</p>
                ) : (
                    <div className="effect-patterns">
                        {this.patterns.map((pattern, index) => pattern.display(index, onUpdate))}
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