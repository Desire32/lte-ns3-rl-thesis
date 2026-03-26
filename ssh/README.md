## Usage
```bash
run-agent {debug,train,infer,random} [options]
```

### Modes

| Mode | Description |
|------|-------------|
| `train` | Train an RL agent against the ns-3 environment |
| `random` | Run a random agent (useful for sanity checks) |
| `infer` | Run inference using a trained checkpoint |
| `debug` | Debug mode |

### Options

| Option | Short | Description |
|--------|-------|-------------|
| `--env-name` | `-n` | Name of the ns-3 environment to start |
| `--max-episode-steps` | `-s` | Number of environment steps per episode |
| `--iterations` | `-i` | Number of training iterations |
| `--single` | `-sg` | Use single-agent mode instead of multi-agent |
| `--training-params` | `-p` | `key=value` pairs overriding training parameters |
| `--checkpoint-path` | `-a` | Path to a checkpoint to load before training |
| `--ns3-settings` | `-c` | `key=value` pairs passed as CLI args to ns-3 |
| `--trainable` | `-t` | RLlib algorithm to use (e.g. `PPO`) |
| `--rollout-fragment-length` | `-rfl` | Rollout fragment length for training |
| `--enable-wandb` | | Enable wandb logging (reads `WANDB_API_KEY`, `WANDB_PROJECT_NAME` from env) |
| `--wandb-project` | `-wp` | Enable wandb logging to this project |
| `--wandb-key` | `-wk` | Enable wandb logging with this API key |

### Examples

**Train for 100 iterations:**
```bash
run-agent train --single -n defiance-lte-ns3-rl-thesis --iterations 100
```

**Quick sanity check with a random agent:**
```bash
run-agent random --single -n defiance-lte-learning --iterations 2
```

**Train with custom ns-3 settings and wandb logging:**
```bash
run-agent train --single -n defiance-lte-ns3-rl-thesis \
    --iterations 100 \
    --ns3-settings topology=multi_bs speed=10 \
    --trainable PPO \
    --enable-wandb
```

**Resume training from a checkpoint:**
```bash
run-agent train --single -n defiance-lte-ns3-rl-thesis \
    --iterations 100 \
    --checkpoint-path /path/to/checkpoint
```
