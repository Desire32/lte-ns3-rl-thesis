"""!Agent leveraging ray to train a single agent for a certain ns3 environment."""

import logging
from typing import Any

import gymnasium as gym
import ray
from gymnasium.wrappers import TimeLimit
from ns3ai_gym_env.envs import Ns3Env
from ray.air.integrations.wandb import WandbLoggerCallback
from ray.rllib.algorithms import AlgorithmConfig
from ray.tune import Tuner, register_env
from ray.tune.impl.config import CheckpointConfig, RunConfig
from ray.tune.registry import get_trainable_cls

# NS3_HOME: path to the ns-3 root directory, used by Ns3Env to locate the simulator binary.
from defiance import NS3_HOME

logger = logging.getLogger(__name__)

from ray.rllib.callbacks.callbacks import RLlibCallback

# ---------------------------------------------------------------------------
# Custom RLlib callback for Weights & Biases (wandb) experiment tracking
# ---------------------------------------------------------------------------


class WandbMetricsCallback(RLlibCallback):
    """
    RLlib lifecycle callback that integrates wandb logging into the training loop.

    Two hook methods are used:
      - on_algorithm_init: runs once when the algorithm object is created.
      - on_train_result:   runs after every training iteration.
    """

    def on_algorithm_init(self, *, algorithm, metrics_logger, **kwargs):
        """
        Initialises a wandb run when the RLlib algorithm starts.
        Builds a human-readable run name from the key hyperparameters
        (learning rate, batch size, rollout fragment length) so runs are
        easy to identify in the wandb dashboard.
        Only creates a new run if one is not already active (guards against
        double-initialisation when ray workers are restarted).
        """
        import wandb

        config = algorithm.config
        run_name = f"PPO__lr{config.lr}__batch{config.train_batch_size}__frag{config.rollout_fragment_length}"
        if not wandb.run:
            wandb.init(project="ns-3-marl", name=run_name)

    def on_train_result(self, *, algorithm, metrics_logger, result, **kwargs):
        """
        Flattens the nested RLlib result dict and logs every numeric metric
        to wandb after each training iteration.

        RLlib returns a deeply nested dict (e.g. result["env_runners"]["episode_reward_mean"]).
        flatten_dict() converts this to a flat dict with slash-separated keys
        (e.g. "env_runners/episode_reward_mean") which wandb displays as grouped metrics.
        NaN values are filtered out (v == v is False for NaN) to avoid wandb errors.
        """
        import wandb

        def flatten_dict(d, prefix=""):
            """Recursively flattens a nested dict into a flat dict with '/' separators."""
            items = {}
            for k, v in d.items():
                key = f"{prefix}/{k}" if prefix else k
                if isinstance(v, dict):
                    items.update(flatten_dict(v, key))
                elif isinstance(v, (int, float)) and v == v:  # exclude NaN (NaN != NaN)
                    items[key] = v
            return items

        metrics = flatten_dict(result)
        wandb.log(metrics)


# ---------------------------------------------------------------------------
# Training configuration factory
# ---------------------------------------------------------------------------


def create_example_training_config(
    env_name: str,
    max_episode_steps: int,
    training_params: dict[str, Any],
    rollout_fragment_length: int,
    trainable: str = "PPO",
    sample_timeout: int = 200,
    **ns3_settings: str,
) -> AlgorithmConfig:
    """!Create an example algorithm config for use with single agent training.

    Registers a custom gymnasium environment called "defiance" with Ray Tune,
    then builds and returns an RLlib AlgorithmConfig (PPO by default).

    @param env_name               ns-3 executable name (e.g. "pc-simulation")
    @param max_episode_steps      maximum number of steps per episode before
                                  TimeLimit truncates the episode
    @param training_params        extra kwargs forwarded to config.training()
                                  (e.g. {"gamma": 0.99})
    @param rollout_fragment_length number of environment steps collected per
                                  rollout worker before sending data to the trainer
    @param trainable              RLlib algorithm name, default "PPO"
    @param sample_timeout         unused parameter (kept for API compatibility)
    @param ns3_settings           additional key=value CLI arguments forwarded
                                  to the ns-3 simulation (e.g. topology="simple")
    """

    def environment_creator(_config: dict[str, Any]) -> Ns3Env:
        """
        Factory function called by each Ray worker to create its own env instance.
        Re-imports ns3ai_gym_env to ensure the Gymnasium env ID is registered
        in the worker process (Ray workers are separate processes).
        Wraps Ns3Env in TimeLimit so episodes are truncated after
        max_episode_steps steps even if the ns-3 simulation hasn't ended.
        """
        import ns3ai_gym_env  # noqa: F401  # re-import to register "ns3ai_gym_env/Ns3-v0"

        return TimeLimit(
            gym.make(
                "ns3ai_gym_env/Ns3-v0",
                targetName=env_name,  # ns-3 binary to run
                ns3Path=NS3_HOME,  # path to ns-3 root
                ns3Settings=ns3_settings,  # CLI args passed to the binary
            ),
            max_episode_steps=max_episode_steps,
        )

    # Register the factory under the "defiance" env ID so RLlib can find it.
    register_env("defiance", environment_creator)

    # Get the default AlgorithmConfig for the chosen algorithm (e.g. PPO).
    config: AlgorithmConfig = get_trainable_cls(trainable).get_default_config()

    # Attach our wandb callback to the algorithm.
    config.callbacks(WandbMetricsCallback)

    return (
        config.environment(env="defiance")
        .training(
            train_batch_size=4000,  # total steps collected before one gradient update
            lr=0.0001,  # learning rate for the policy network
            **training_params,  # caller-supplied overrides (gamma, clip_param, etc.)
        )
        .resources(num_gpus=0)  # CPU-only training
        .env_runners(
            num_env_runners=1,  # one rollout worker process
            num_envs_per_env_runner=1,  # one ns-3 instance per worker
            sample_timeout_s=120,  # kill worker if it doesn't respond within 120 s
            create_env_on_local_worker=False,  # don't create env on the trainer process
            rollout_fragment_length=rollout_fragment_length or 1000,  # steps per rollout
        )
    )


# ---------------------------------------------------------------------------
# Training entry point
# ---------------------------------------------------------------------------


def start_training(
    iterations: int,
    config: AlgorithmConfig,
    trainable: str = "PPO",
    load_checkpoint_path: str | None = None,
    wandb_logger: WandbLoggerCallback | None = None,
) -> None:
    """!Start a ray training session with the given algorithm config.

    Initialises the Ray cluster (local mode if no cluster is running),
    creates a Tune Tuner, and runs training for `iterations` iterations.
    Saves a checkpoint at the end of training.
    Shuts down Ray cleanly on success or logs the exception on failure.

    @param iterations           number of training iterations to run
    @param config               AlgorithmConfig produced by create_example_training_config
    @param trainable            RLlib algorithm name, must match the one used in config
    @param load_checkpoint_path checkpoint resumption is not supported for single-agent
                                (logged as info and ignored)
    @param wandb_logger         optional WandbLoggerCallback added to Tune's RunConfig
    """
    logger.info(
        "Loading checkpoints is not supported for single agent: %s",
        load_checkpoint_path,
    )
    try:
        # Start Ray — connects to an existing cluster or starts a local one.
        ray.init()
        logger.info("Training...")

        Tuner(
            trainable,
            run_config=RunConfig(
                # Stop condition: run exactly `iterations` training iterations.
                stop={"training_iteration": iterations},
                # Save one checkpoint when training finishes.
                checkpoint_config=CheckpointConfig(checkpoint_at_end=True),
                # Attach wandb logger if provided (logs trial-level metrics).
                callbacks=[wandb_logger] if wandb_logger else [],
            ),
            # config.to_dict() converts the AlgorithmConfig to a plain dict
            # that Tune uses as the hyperparameter search space (single point here).
            param_space=config.to_dict(),
        ).fit()

        ray.shutdown()
    except Exception:
        # Log the full traceback without re-raising so the process exits cleanly.
        logger.exception("Exception occurred!")
