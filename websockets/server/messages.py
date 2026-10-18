from dataclasses import dataclass
from enum import StrEnum
from bridge import VALID_ROLES

import json


class MessageType(StrEnum):
    REGISTRATION = "registration"


@dataclass
class RegistrationMessage:
    type: str
    client_id: str
    role: str

    @classmethod
    def from_json(cls, raw: str) -> RegistrationMessage:
        data = json.loads(raw)

        # Ensure that this is a JSON object
        if not isinstance(data, dict):
            raise ValueError("Message must be a JSON object")

        # Ensure message type is correct
        if data.get("type") != MessageType.REGISTRATION:
            raise ValueError("Expected registration message")

        client_id = data.get("client_id")
        role = data.get("role")

        if not isinstance(client_id, str) or not client_id.strip():
            raise ValueError("client_id must be a nonempty string")

        if role not in VALID_ROLES:
            raise ValueError("Invalid client role")

        return cls(
            type="register",
            client_id=client_id,
            role=role,
        )
