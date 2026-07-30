import 'package:flutter/material.dart';
import '../api/notifications_client.dart';

// StatefulWidget because this screen has mutable state that changes over
// time as the user interacts with it: the text typed into the fields (held
// by the TextEditingControllers below) and whether a submit request is
// currently in flight (_isSubmitting, used to show a spinner and disable the
// button). A StatelessWidget can't hold state like this across rebuilds —
// State lives in the accompanying State<T> object, which Flutter keeps
// alive across build() calls until the widget is removed from the tree.
class CreateNotificationScreen extends StatefulWidget {
  const CreateNotificationScreen({super.key});

  @override
  State<CreateNotificationScreen> createState() =>
      _CreateNotificationScreenState();
}

class _CreateNotificationScreenState extends State<CreateNotificationScreen> {
  // GlobalKey<FormState> lets us reach into the Form widget below (e.g. call
  // its validate() method) from outside the widget tree, in the submit
  // handler. This is the standard Flutter pattern for form validation.
  final _formKey = GlobalKey<FormState>();

  // TextEditingController owns the text in a TextFormField and lets us read
  // it (`.text`) whenever we need to, instead of wiring up onChanged
  // callbacks to track it ourselves.
  final _titleController = TextEditingController();
  final _bodyController = TextEditingController();

  // Tracks whether the create request is in flight, so the button can show
  // a spinner and ignore taps while a request is already running.
  bool _isSubmitting = false;

  @override
  void dispose() {
    // Controllers hold native resources and must be disposed when the
    // State is destroyed, or they leak. This mirrors freeing a resource in
    // a C++ destructor — dispose() is Flutter's equivalent hook.
    _titleController.dispose();
    _bodyController.dispose();
    super.dispose();
  }

  Future<void> _submit() async {
    // validate() runs every TextFormField's validator and returns false if
    // any of them fail (Flutter also shows the error text under the field
    // automatically). No point calling the backend if the form is invalid.
    if (!_formKey.currentState!.validate()) {
      return;
    }

    setState(() => _isSubmitting = true);

    try {
      await createNotification(_titleController.text, _bodyController.text);

      // mounted guards against calling setState/Navigator on a State whose
      // widget has already been removed from the tree (e.g. the user
      // navigated away while the request was still in flight).
      if (!mounted) return;

      // Pop back to NotificationsScreen, passing `true` as the result so it
      // knows to refetch and show the new item.
      Navigator.pop(context, true);
    } catch (e) {
      if (!mounted) return;
      setState(() => _isSubmitting = false);

      // A SnackBar is the minimal way to surface a transient error message
      // without leaving the form — the user can just fix the input and
      // retry, their typed text is untouched.
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('$e')),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Nova notificação'),
        backgroundColor: const Color(0xFF1B1B2F), // dark navy
        foregroundColor: Colors.white,
      ),
      backgroundColor: const Color(0xFFF5F5F0), // off-white

      body: Form(
        key: _formKey,
        child: ListView(
          padding: const EdgeInsets.all(16),
          children: [
            TextFormField(
              controller: _titleController,
              decoration: const InputDecoration(
                labelText: 'Título',
                filled: true,
                fillColor: Colors.white,
                border: OutlineInputBorder(),
              ),
              // Mirrors the backend's own non-empty validation (see
              // create_notification_request.hpp) so the user gets instant
              // feedback instead of waiting on a round trip for the obvious
              // case. The backend still re-validates — this is only a UX
              // shortcut, never the source of truth.
              validator: (value) {
                if (value == null || value.trim().isEmpty) {
                  return 'Título não pode ser vazio.';
                }
                return null;
              },
            ),
            const SizedBox(height: 16),
            TextFormField(
              controller: _bodyController,
              decoration: const InputDecoration(
                labelText: 'Mensagem',
                filled: true,
                fillColor: Colors.white,
                border: OutlineInputBorder(),
              ),
              maxLines: 4,
              validator: (value) {
                if (value == null || value.trim().isEmpty) {
                  return 'Mensagem não pode ser vazia.';
                }
                return null;
              },
            ),
            const SizedBox(height: 24),
            ElevatedButton(
              onPressed: _isSubmitting ? null : _submit,
              style: ElevatedButton.styleFrom(
                backgroundColor: const Color(0xFF1B1B2F),
                foregroundColor: Colors.white,
                padding: const EdgeInsets.symmetric(vertical: 14),
              ),
              child: _isSubmitting
                  ? const SizedBox(
                      height: 20,
                      width: 20,
                      child: CircularProgressIndicator(
                        strokeWidth: 2,
                        color: Colors.white,
                      ),
                    )
                  : const Text('Criar'),
            ),
          ],
        ),
      ),
    );
  }
}
