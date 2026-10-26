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
                    <table>
                        {this.patterns.map((pattern, index) => pattern.display(index, onUpdate))}
                    </table>
                )}

                <p>(Looping: {this.isLooping ? "Yes" : "No"})</p>
            </div>
        );
    }
}