import struct
import unittest
from inspect_crisol_records import inspect, achievements


def text(value):
    raw = value.encode('ascii') + b'\0'
    return struct.pack('<i', len(raw)) + raw


def record(payload=b'\0' * 13):
    return (text('/Game/Test/CR_Teleport_BP.CR_Teleport_BP_C') +
            text('Test.CR_Teleport_BP.SaveComp.1') + text('CR_Teleport_BP_C_123') +
            struct.pack('<10dII', 0, 0, 0, 1, 10, 20, 30, 1, 1, 1, 1, len(payload)) + payload)


class SafetyTests(unittest.TestCase):
    def test_rejects_every_truncated_record(self):
        data = record()
        for length in range(len(data)):
            self.assertEqual(inspect(data[:length]), [], length)
        self.assertEqual(len(inspect(data)), 1)

    def test_references_alone_are_not_actor_states(self):
        self.assertEqual(inspect(text('/Game/Test/CR_Teleport_BP.CR_Teleport_BP_C')), [])

    def test_binary_payload_is_not_scanned_as_another_record(self):
        # An ordinary ASCII reference in payload must not produce a second actor.
        data = record(text('/Game/Test/CR_Teleport_BP.CR_Teleport_BP_C'))
        self.assertEqual(len(inspect(data)), 1)

    def test_duplicate_objects_remain_visible(self):
        self.assertEqual(len(inspect(record()+record())), 2)

    def test_map_authored_instance_name_is_supported(self):
        data = record().replace(text('CR_Teleport_BP_C_123'), text('MapAuthoredDoor'))
        self.assertEqual(len(inspect(data)), 1)

    def test_achievement_ids_and_utf16_survive(self):
        wide = 'Souvenir'.encode('utf-16-le') + b'\0\0'
        data = (struct.pack('<ii', 0, 2) + text('ach_all_solari') +
                struct.pack('<i', 1) + text('1') + text('echoes') +
                struct.pack('<ii', 1, -9) + wide)
        result = achievements(data)
        self.assertEqual(result['groups']['ach_all_solari'], ['1'])
        self.assertEqual(result['groups']['echoes'], ['Souvenir'])
        self.assertEqual(result['parsed_bytes'], len(data))
        for end in range(len(data)):
            with self.assertRaises(ValueError):
                achievements(data[:end])


if __name__ == '__main__':
    unittest.main()
