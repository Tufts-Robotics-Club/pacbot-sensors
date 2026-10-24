from dataclasses import dataclass
from enum import StrEnum
from bridge import VALID_ROLES
from abc import ABC, abstractmethod
from typing import Self, ClassVar
from dataclasses import asdict, dataclass

import json


class MessageType(StrEnum):
    """Represents all supported message types the server can interpret."""

    REGISTRATION = "registration"
    ERROR = "error"
    INFO = "info"
    INSTRUCTION = "instruction"


@dataclass
class BaseMessage(ABC):
    """The base class that all message types inherit from."""

    # TODO: Add timestamp support
    # TODO: Add client identification support
    # TODO: Add message code support?

    TYPE: ClassVar[str]

    def to_dict(self) -> dict:
        """Convert the message to a JSON-compatible dictionary."""
        return {
            "type": self.TYPE,
            **asdict(self),
        }

    def to_json(self) -> str:
        """Serialize the message to a JSON string."""
        return json.dumps(self.to_dict())

    @classmethod
    def from_json(cls, raw: str):
        """Validates the JSON structure and message type before allowing its use."""

        try:
            data = json.loads(raw)
        except json.JSONDecodeError as exc:
            raise ValueError("Invalid JSON") from exc

        if not isinstance(data, dict):
            raise ValueError("Message must be a JSON object")

        if data.get("type") != cls.TYPE:
            raise ValueError(f"Expected message type '{cls.TYPE}'")

        return cls.from_dict(data)

    @classmethod
    @abstractmethod
    def from_dict(cls, data: dict) -> Self:
        """Validate fields and construct the message."""
        pass


@dataclass
class TextMessage(BaseMessage):
    """Base class for messages containing a text payload."""

    message: str

    @classmethod
    def from_dict(cls, data: dict) -> Self:
        message = data.get("message")

        if not isinstance(message, str):
            raise ValueError("message must be a string")

        return cls(message=message)


@dataclass
class InfoMessage(TextMessage):
    """Represents a message containing information as a text payload"""

    TYPE = MessageType.INFO


@dataclass
class ErrorMessage(TextMessage):
    """Represents a message containing an error as a text payload"""

    TYPE = MessageType.INFO


@dataclass
class RegistrationRequestMessage(BaseMessage):
    client_id: str
    role: str

    TYPE = MessageType.REGISTRATION

    @classmethod
    def from_dict(cls, data: dict) -> Self:
        client_id = data.get("client_id")
        role = data.get("role")

        if not isinstance(client_id, str) or not client_id.strip():
            raise ValueError("client_id must be a nonempty string")

        if role not in VALID_ROLES:
            raise ValueError("Invalid client role")

        return cls(
            client_id=client_id,
            role=role,
        )


@dataclass
class RegistrationResponseMessage(BaseMessage):
    """Reports whether a client's registration succeeded."""

    success: bool
    message: str

    TYPE = "registration_response"

    @classmethod
    def from_dict(cls, data: dict) -> Self:
        success = data.get("success")
        message = data.get("message")

        if not isinstance(success, bool):
            raise ValueError("success must be a boolean")

        if not isinstance(message, str):
            raise ValueError("message must be a string")

        return cls(success=success, message=message)
